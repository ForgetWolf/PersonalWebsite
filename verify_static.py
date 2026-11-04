"""Offline checks for the deployable static tree; no HTTP/browser required."""
from pathlib import Path
from html.parser import HTMLParser
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
DIST = ROOT / 'dist'
S = '{http://www.w3.org/2000/svg}'
EXTERNAL = ('http://', 'https://', '#', 'mailto:')

class CheckHTML(HTMLParser):
    def __init__(self, page):
        super().__init__()
        self.page = page

    def handle_starttag(self, tag, attrs):
        assert tag not in ('script', 'style'), tag
        for key, value in attrs:
            assert key != 'style' and not key.startswith('on'), (key, value)
            if key == 'href' and not value.startswith(EXTERNAL):
                target = (self.page.parent / value).resolve()
                assert target.is_file() or (target / 'index.html').is_file(), (self.page, value)

for path in DIST.rglob('*.html'):
    CheckHTML(path).feed(path.read_text(encoding='utf-8'))

def canon(path):
    """Normalise page-relative hrefs so equivalent pages compare equal."""
    text = path.read_text(encoding='utf-8')
    def fix(match):
        value = match.group(1)
        if value.startswith(EXTERNAL):
            return match.group(0)
        target = (path.parent / value).resolve()
        try:
            return 'href="/' + target.relative_to(DIST).as_posix() + '"'
        except ValueError:
            return match.group(0)
    return re.sub(r'href="([^"]+)"', fix, text)

assert canon(DIST/'index.html') == canon(DIST/'desktop/index.html')
for layout in ('desktop', 'mobile'):
    default = (DIST/layout/'index.html').read_text(encoding='utf-8')
    assert 'data-order="en,ru,zh"' in default
    assert len(list((DIST/layout/'orders').glob('*/index.html'))) == 6
    for path in (DIST/layout/'orders').glob('*/index.html'):
        text = path.read_text(encoding='utf-8')
        scene = ET.fromstring(text[text.index('<svg'):text.rindex('</svg>')+6])
        landing = scene.find(S+"g[@id='landing']")
        assert landing.get('display') == 'none'
        assert 'randomPage.click' in landing.find(S+'set').get('begin')
        assert landing.get('data-output') == (ROOT/'random/message.md').read_text(encoding='utf-8')
        pointer = scene.find('.//'+S+"g[@id='bf-pointer-motion']/"+S+'animateTransform')
        keys = [float(k) for k in pointer.get('keyTimes').split(';')]
        assert all(a < b for a,b in zip(keys,keys[1:]))
        assert len(keys) > 100 and keys[-2] > .9
for name in ('about','friends','contact','en','ru','zh','helloworld','carousel','random'):
    assert (DIST/name/'index.html').is_file()
assert not any(path.suffix in ('.exe','.cpp','.py') for path in DIST.rglob('*'))
print(f'Validated {len(list(DIST.rglob("*.html")))} static pages, links, default order and animation tracks; no JS/CSS/CGI.')
