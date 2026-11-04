"""Build deployable HTML/SVG files. No CGI executables are invoked."""
from pathlib import Path
import argparse
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
DIST = ROOT / 'dist'

def document(svg, lang='en', title='ForgetWolf'):
    return f'''<!doctype html>
<html lang="{lang}" id="randomPage"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>{title}</title></head><body>{svg}</body></html>
'''

def write(route, text):
    target = DIST / route / 'index.html'
    target.parent.mkdir(parents=True, exist_ok=True)
    # Same-origin links become page-relative so the export works at any base path.
    text = text.replace('https://forgetwolf.net/', '/')
    depth = len([part for part in route.split('/') if part])
    prefix = '../' * depth
    text = text.replace('href="/', 'href="' + prefix)
    text = text.replace('url=/', 'url=' + prefix)
    text = text.replace('href=""', 'href="./"')
    target.write_text(text, encoding='utf-8')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--export-only', action='store_true', help='Use already generated SVGs/pages')
    args = parser.parse_args()
    if not args.export_only:
        for script in ('random/generate.py', 'mobile/generate.py', 'pages/generate.py'):
            subprocess.run([sys.executable, str(ROOT / script)], cwd=ROOT, check=True)
    for layout, source in (('desktop', 'random'), ('mobile', 'mobile')):
        for svg in sorted((ROOT / source).glob('*.svg')):
            page = document(svg.read_text(encoding='utf-8'))
            write(f'{layout}/orders/{svg.stem}', page)
            if svg.stem == 'en-ru-zh':
                write(layout, page)
                if layout == 'desktop': write('', page)
    for route in ('about', 'friends', 'contact'):
        write(route, (ROOT / 'pages' / route / 'index.html').read_text(encoding='utf-8'))
    for lang, header, delimiter in (('en','greeting.hpp','GREETING'), ('ru','russian.hpp','RUSSIAN'), ('zh','chinese.hpp','CHINESE')):
        source = (ROOT / header).read_text(encoding='utf-8')
        match = re.search(r'R"' + delimiter + r'\((.*?)\)' + delimiter + '"', source, re.S)
        if not match: raise ValueError(f'Missing SVG literal in {header}')
        write(lang, document(match.group(1), lang))
    carousel = document((ROOT / 'carousel/carousel.svg').read_text(encoding='utf-8'))
    write('carousel', carousel)
    for step in range(3): write(f'carousel/{step}', carousel)
    write('helloworld', document('hello world', title='Hello World'))
    write('random', '<!doctype html><html><head><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=/desktop/"><title>ForgetWolf</title></head><body><a href="/desktop/">Main</a></body></html>')
    print('Static export ready: dist/; default language order en-ru-zh.')

if __name__ == '__main__': main()
