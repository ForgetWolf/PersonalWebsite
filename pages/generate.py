"""Static subpages with the same DIN heading and native SVG navigation."""
from pathlib import Path
from html import escape
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / ".tools"))
sys.path.insert(0, str(ROOT / "random"))
import markdown
from info_toggle import add_info_toggle
from din_lettering import outline_text

S = "{http://www.w3.org/2000/svg}"
ET.register_namespace("",S[1:-1])

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
    body = markdown.markdown("\n".join(lines), extensions=["extra", "nl2br"])
    return metadata, body

for name in ("about","friends","contact"):
    metadata, body = read_page(name)
    title = name.title()
    root = ET.Element(S+"svg", {"width":"100%","height":"100"})
    heading = ET.SubElement(root,S+"svg",{
        "id":"page-heading","x":"5%","y":"0","width":"90%","height":"76.8",
        "viewBox":"0 0 360 76.8","preserveAspectRatio":"xMinYMin meet",
    })
    ET.SubElement(heading,S+"text",{
        "x":"0","y":"57.6","font-size":"57.6","fill":"CanvasText",
    }).text = title
    add_info_toggle(root,active_page=name)
    outline_text(root)
    svg = ET.tostring(root,encoding="unicode")
    html = f'''<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>{title} - ForgetWolf</title>
</head><body>
{svg}
<table width="90%" align="center" border="0" cellpadding="0" cellspacing="0"><tbody><tr><td>
<header>
<p>{escape(metadata.get('desc', ''))}</p>
<p><small>最后编辑时间：<time>{escape(metadata.get('date', metadata.get('data', '')))}</time></small></p>
</header>
<main>{body}</main>
</td></tr></tbody></table>
</body></html>
'''
    (ROOT/"pages"/name/"index.html").write_text(html,encoding="utf-8")
    links = list(root.iter(S+"a"))
    assert [a.get('aria-label') for a in links] == ['Main' if label.lower()==name else label for label in ('About','Blog','Friends','Contact')]
    assert next(a for a in links if a.get('aria-label')=='Main').get('href')=='https://forgetwolf.net/'
print('Built About, Friends and Contact from source Markdown; main unchanged.')
