"""Offline SVG/SMIL timeline snapshots for visual QA; no browser required.

Supports this project's linear stroke/opacity animations and visibility sets.
Resolves normalized dash lengths to real units for the static resvg renderer.
"""
from pathlib import Path
import copy
import io
import math
import re
import xml.etree.ElementTree as E
from PIL import Image, ImageDraw
import resvg_py

ROOT = Path(__file__).resolve().parent
s = (ROOT / 'chinese.hpp').read_text(encoding='utf-8')
root = E.fromstring(s[s.index('<svg'):s.rindex('</svg>')+6])
root.set('width', '1100');root.set('height', '240')
root.find('{http://www.w3.org/2000/svg}svg').set('y','10%')
root.find('{http://www.w3.org/2000/svg}svg').set('height','80%')
times = [.4, 1.1, 2.0, 2.8, 4.2, 6.1, 7.4, 8.6, 9.4]
sheet = Image.new('RGB',(1100,270*len(times)),'white')
draw=ImageDraw.Draw(sheet)
for i,t in enumerate(times):
    r=copy.deepcopy(root)
    for el in r.iter():
        for a in list(el):
            name=a.tag.rsplit('}',1)[-1]
            if name not in ('animate','set'):continue
            start=float(a.get('begin')[:-1])
            if name=='set':
                if t>=start:el.set(a.get('attributeName'),a.get('to'))
            elif t>=start:
                duration=float(a.get('dur')[:-1]);fraction=min(1,(t-start)/duration)
                value=float(a.get('from'))+(float(a.get('to'))-float(a.get('from')))*fraction
                el.set(a.get('attributeName'),str(value))
            el.remove(a)
        if el.get('pathLength'):
            # All animated paths here contain moveto/lineto commands only.
            values=list(map(float,re.findall(r'-?\d+(?:\.\d+)?',el.get('d'))))
            points=list(zip(values[::2],values[1::2]))
            actual=sum(math.dist(a,b) for a,b in zip(points,points[1:]))
            ratio=actual/float(el.get('pathLength'))
            dash=[float(v)*ratio for v in el.get('stroke-dasharray').split()]
            el.set('stroke-dasharray',' '.join(map(str,dash)))
            el.set('stroke-dashoffset',str(float(el.get('stroke-dashoffset'))*ratio))
            el.attrib.pop('pathLength')
    markup=E.tostring(r,encoding='unicode').replace('CanvasText','#000')
    png=resvg_py.svg_to_bytes(svg_string=markup,background='white')
    im=Image.open(io.BytesIO(png)).convert('RGB')
    sheet.paste(im,(0,270*i+30));draw.text((15,270*i+8),f'{t:.1f} s',fill='black')
    if t==8.6:
        im.save(ROOT/'lettering/zh-final-proof.png')
sheet.save(ROOT/'lettering/zh-timeline-proof.png')
print('Saved static timeline frames and completed lettering preview.')
