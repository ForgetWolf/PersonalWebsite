"""Static subpages with the same DIN heading and native SVG navigation."""
from pathlib import Path
from html import escape
import re
import sys
from urllib.parse import urlsplit
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / ".tools"))
sys.path.insert(0, str(ROOT / "random"))
import markdown
from markdown.extensions import Extension
from markdown.inlinepatterns import SimpleTagInlineProcessor
from info_toggle import add_info_toggle
from din_lettering import outline_text

S = "{http://www.w3.org/2000/svg}"
ET.register_namespace("",S[1:-1])

FRIEND_URL = re.compile(
    r"(?:https?://)?(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z]{2,}"
    r"(?::[0-9]+)?(?:[/?#][^\s<>]*)?",
    re.IGNORECASE,
)
FRIEND_ICONS = {
    "linusaur.net": ("https://linusaur.net/favicon.ico", "image/x-icon"),
    "ymckc.monster": ("https://ymckc.monster/favicon.ico", "image/gif"),
}


def friend_entry(label):
    url = label if "://" in label else "https://" + label
    host = urlsplit(url).hostname
    initial = f'<span class="friend-initial">{escape(host[0].upper())}</span>'
    favicon = FRIEND_ICONS.get(host)
    icon = initial
    if favicon:
        icon_url, image_type = favicon
        icon = (
            f'<object class="friend-icon" data="{escape(icon_url)}" type="{image_type}">'
            f'{initial}</object>'
        )
    return (
        f'<div class="friend-entry"><div class="friend-mark" aria-hidden="true">{icon}</div>'
        f'<a href="{escape(url)}">{escape(label)}</a></div>'
    )


class StrikethroughExtension(Extension):
    def extendMarkdown(self, md):
        md.ESCAPED_CHARS.append("~")
        pattern = r"(?<!~)(~~)(?=\S)(.+?)(?<=\S)~~(?!~)"
        md.inlinePatterns.register(
            SimpleTagInlineProcessor(pattern, "del"), "strikethrough", 65
        )


def read_page(name):
    """Read simple front matter without interpreting dates as YAML objects."""
    text = (ROOT / "source" / f"{name}.md").read_text(encoding="utf-8-sig")
    lines = text.splitlines()
    metadata = {}
    if lines and lines[0].strip() == "---":
        end = next(i for i in range(1, len(lines)) if lines[i].strip() == "---")
        for line in lines[1:end]:
            key, separator, value = line.partition(":")
            if separator:
                value = value.strip()
                if value.startswith("{{") and value.endswith("}}"):
                    value = value[2:-2].strip()
                metadata[key.strip()] = value
        lines = lines[end + 1:]
    source = "\n".join(lines)
    if name == "friends":
        paragraphs = re.split(r"(\n[ \t]*\n+)", source)
        for index in range(0, len(paragraphs), 2):
            # A friend entry may end with a URL below its description.
            description, separator, label = paragraphs[index].strip().rpartition("\n")
            label = label.strip()
            if FRIEND_URL.fullmatch(label):
                paragraphs[index] = description + ("\n\n" if separator else "") + friend_entry(label)
        source = "".join(paragraphs)
    body = markdown.markdown(source, extensions=["extra", "nl2br", StrikethroughExtension()])
    return metadata, body

for name in ("about","friends","contact"):
    metadata, body = read_page(name)
    title = name.title()
    title_svg = ET.Element(S+"svg", {"width":"100%","height":"100"})
    heading = ET.SubElement(title_svg,S+"svg",{
        "id":"page-heading","x":"5%","y":"0","width":"90%","height":"76.8",
        "viewBox":"0 0 360 76.8","preserveAspectRatio":"xMinYMin meet",
    })
    ET.SubElement(heading,S+"text",{
        "x":"0","y":"57.6","font-size":"57.6","fill":"CanvasText",
    }).text = title
    root = ET.Element(S+"svg", {"width":"100%","height":"100"})
    add_info_toggle(root,active_page=name)
    toggle = root.find(S+"svg[@id='info-toggle']")
    toggle.insert(0, ET.Element(S+"rect", {
        "x": "-30", "y": "-30", "width": "60", "height": "60", "rx": "8", "fill": "Canvas",
    }))
    outline_text(title_svg)
    title_markup = ET.tostring(title_svg,encoding="unicode")
    outline_text(root)
    svg = ET.tostring(root,encoding="unicode")
    html = f'''<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>{title} - ForgetWolf</title>
<link rel="stylesheet" href="../page.css">
</head><body>
<div class="page-title">{title_markup}</div>
<div class="page-navigation">{svg}</div>
<div class="page-copy">
<header class="page-summary">
<p>{escape(metadata.get('desc', ''))}</p>
<p class="page-date" lang="zh-Hans">最后编辑时间：<time>{escape(metadata.get('date', metadata.get('data', '')))}</time></p>
</header>
<main>{body}</main>
</div>
</body></html>
'''
    output = ROOT / "pages" / name / "index.html"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(html, encoding="utf-8")
    links = list(root.iter(S+"a"))
    assert [a.get('aria-label') for a in links] == ['Main' if label.lower()==name else label for label in ('About','Blog','Friends','Contact')]
    assert next(a for a in links if a.get('aria-label')=='Main').get('href')=='https://forgetwolf.net/'
print('Built About, Friends and Contact from source Markdown; main unchanged.')
