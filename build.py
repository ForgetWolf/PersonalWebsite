from pathlib import Path
import shutil, subprocess, sys, re
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parent
S = "{http://www.w3.org/2000/svg}"
ET.register_namespace("", S[1:-1])
for script in (
    ()
    if "--export-only" in sys.argv
    else ("random/generate.py", "mobile/generate.py", "pages/generate.py")
):
    subprocess.run([sys.executable, str(R / script)], cwd=R, check=True)
d = R / "dist"
d.mkdir(exist_ok=True)
font_directory = d / "fonts"
font_directory.mkdir(exist_ok=True)
for filename in ("IBM-VGA-8x16.woff", "VGA-LICENSE.txt", "VGA-README.txt", "Open Font License.txt"):
    shutil.copy2(R / "random/fonts" / filename, font_directory / filename)

shutil.copy2(R / "random/fonts/din1451alt.ttf", font_directory / "din1451alt.ttf")
for filename in ("NotoSerifSC-Regular.woff2", "NotoSerifSC-Regular.woff", "NotoSerifSC-OFL.txt"):
    shutil.copy2(R / "pages/fonts" / filename, font_directory / filename)
shutil.copy2(R / "pages/page.css", d / "page.css")

css = """@font-face{font-family:"VGA Terminal";src:url("fonts/IBM-VGA-8x16.woff") format("woff");font-weight:400;font-style:normal;font-display:block}
@property --pick{syntax:"<integer>";inherits:true;initial-value:0}
:root{--pick:0;color-scheme:light dark}
@supports (opacity:random(0,5,1)){:root{--pick:random(0,5,1)}}
body{margin:0;background:Canvas;color:CanvasText}
.viewport{position:relative;overflow:hidden;width:100%;height:100vh;height:100svh}
.wide,.narrow{height:100%}.narrow{display:none}
svg{max-width:100%}text{user-select:text;-webkit-user-select:text}
.welcome-order{pointer-events:none}
@media(max-width:700px){.wide{display:none}.narrow{display:block}}
@media(orientation:landscape) and (max-height:459px){
  .terminal-fit{transform-box:view-box;transform-origin:center;transform:translateY(22px) scale(.8)}
}
@media(orientation:landscape) and (max-height:379px){
  .terminal-fit{transform:translateY(20px) scale(.68)}
}
@media(orientation:landscape) and (max-height:329px){
  .terminal-fit{transform:translateY(20px) scale(.6)}
}
"""
orders = ("en-ru-zh", "en-zh-ru", "ru-en-zh", "ru-zh-en", "zh-en-ru", "zh-ru-en")
for index in range(len(orders)):
    css += f".welcome-{index}{{opacity:calc(1 - max(var(--pick) - {index}, {index} - var(--pick)))}}\n"


def prefix_ids(root, prefix, scene_id):
    ids = {node.get("id"): prefix + node.get("id") for node in root.iter() if node.get("id")}
    ids["randomScene"] = scene_id
    for node in root.iter():
        for key, value in list(node.attrib.items()):
            if key == "id":
                node.set(key, ids[value])
                continue
            value = value.replace("randomPage.click", "sharedWelcome.click")
            value = re.sub(r"url\(#([^)]+)\)", lambda m: "url(#" + ids.get(m[1], m[1]) + ")", value)
            value = re.sub(r"([\w-]+)\.(click|begin|end)",
                           lambda m: ids.get(m[1], m[1]) + "." + m[2], value)
            if key == "href":
                if value.startswith("https://forgetwolf.net/"):
                    route = value.removeprefix("https://forgetwolf.net/")
                    value = "./" + (route + "/" if route else "")
                if value.startswith("#"):
                    value = "#" + ids.get(value[1:], value[1:])
            node.set(key, value)


def render_layout(layout, folder):
    scene_id = f"p0{layout}_randomScene"
    root = ET.parse(R / folder / (orders[0] + ".svg")).getroot()
    landing = root.find(S + "g[@id='landing']")
    stage = landing.find(".//" + S + "svg[@id='terminal-stage']")
    stage.set("class", "terminal-stage")
    stage_position = next(group for group in landing if stage in list(group))
    stage_index = list(landing).index(stage_position)
    landing.remove(stage_position)
    fit = ET.Element(S + "g", {"class": "terminal-fit"})
    fit.append(stage_position)
    landing.insert(stage_index, fit)
    first_welcome = root.find(S + "g")
    prefix_ids(root, f"p0{layout}_", scene_id)
    root.remove(first_welcome)
    # Each greeting keeps its own stroke timing. Both click and the common
    # finish time are handled by the same root SVG as the single terminal.
    for index, order in enumerate(orders):
        if index == 0:
            welcome = first_welcome
        else:
            welcome = ET.parse(R / folder / (order + ".svg")).getroot().find(S + "g")
            prefix_ids(welcome, f"p{index}{layout}_", scene_id)
        wrapper = ET.Element(S + "g", {"class": f"welcome-order welcome-{index}", "data-order": order})
        wrapper.append(welcome)
        root.insert(index + 1, wrapper)
    root.set("height", "100%")
    return f'<div class="{layout}">' + ET.tostring(root, encoding="unicode") + "</div>"


html = (
    '<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light dark"><title>ForgetWolf</title><link rel="stylesheet" href="site.css"><body id="sharedWelcome"><main class="viewport">'
    + render_layout("wide", "random")
    + render_layout("narrow", "mobile")
    + "</main></body></html>"
)
(d / "index.html").write_text(html, encoding="utf-8")
(d / "site.css").write_text(css, encoding="utf-8")
for name in ("about", "friends", "contact"):
    target = d / name
    target.mkdir(exist_ok=True)
    text = (
        (R / "pages" / name / "index.html")
        .read_text(encoding="utf-8")
        .replace("https://forgetwolf.net/", "../")
    )
    for route in ("about", "friends", "contact"):
        text = text.replace(f'href="../{route}"', f'href="../{route}/"')
    (target / "index.html").write_text(text, encoding="utf-8")
print("Built adaptive static site", len(html.encode()), "bytes")
