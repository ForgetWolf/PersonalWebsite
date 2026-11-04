"""Offline slow-motion proof of the exact 我/是 SMIL visibility events."""
from pathlib import Path
import copy
import io
import xml.etree.ElementTree as E
from PIL import Image, ImageDraw
import resvg_py

ROOT=Path(__file__).resolve().parent
NS={'s':'http://www.w3.org/2000/svg'}
source=E.parse(ROOT/'lettering/wo.svg').getroot()
masks=source.findall('.//s:mask',NS)

def point(i,fraction=1):
    mask=masks[i]
    return float(mask.get('data-begin'))+float(mask.get('data-duration'))*fraction-1e-7

def snapshot(t):
    root=copy.deepcopy(source)
    for el in root.iter():
        for a in list(el):
            if a.tag.endswith('}set'):
                if t>=float(a.get('begin')[:-1]):
                    el.set(a.get('attributeName'),a.get('to'))
                el.remove(a)
            elif a.tag.endswith('}animate'):
                start=float(a.get('begin')[:-1])
                if t>=start:
                    f=min(1,(t-start)/float(a.get('dur')[:-1]))
                    value=float(a.get('from'))+(float(a.get('to'))-float(a.get('from')))*f
                    el.set(a.get('attributeName'),str(value))
                el.remove(a)
    data=resvg_py.svg_to_bytes(svg_string=E.tostring(root,encoding='unicode'),background='white')
    return Image.open(io.BytesIO(data)).convert('RGB')

checkpoints=[(point(1),'vertical complete'),(point(2),'ti complete')]
checkpoints += [(point(3,f),f'diagonal/return {f:.0%}') for f in [.6,.72,.8,.9,1]]
checkpoints += [(point(i),f'after movement {i+1}') for i in range(4,12)]
sheet=Image.new('RGB',(1030,340*((len(checkpoints)+1)//2)),'white')
draw=ImageDraw.Draw(sheet)
for k,(t,label) in enumerate(checkpoints):
    x=k%2*515;y=k//2*340
    sheet.paste(snapshot(t),(x,y+25));draw.text((x+6,y+6),label,fill='black')
sheet.save(ROOT/'lettering/wo-shi-review.png')
first=float(source.get('data-timeline-start'));end=float(source.get('data-timeline-end'))
frames=[snapshot(first+(end-first)*i/119) for i in range(120)]
frames[0].save(ROOT/'lettering/wo-shi-slow-proof.gif',save_all=True,
    append_images=frames[1:]+[frames[-1]]*10,duration=80,loop=0)
print('Rendered 15 checkpoints and 120 slow-motion frames.')
