"""Reflow existing animated strokes without touching their paths or timings."""
import re
import xml.etree.ElementTree as E
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
S = "{http://www.w3.org/2000/svg}"


def coordinates(path):
    values = list(map(float, re.findall(r'-?\d+(?:\.\d+)?(?:e[+-]?\d+)?', path)))
    return list(zip(values[::2], values[1::2]))


def reflow_welcomes(root):
    for language in (n for n in root.iter(S+'svg') if n.get('data-language')):
        code = language.get('data-language')
        frame = language.find(S+'svg'); ink = frame.find(S+'g')
        frame.attrib.update({'x':'8%', 'y':'18%', 'width':'84%', 'height':'64%'})
        if code == 'zh':
            groups = ink.findall(S+'g')
            assert len(groups) == 4
            rows = [E.SubElement(ink,S+'g',{'transform':shift}) for shift in
                    ('translate(-85 0)','translate(-237.5 115)','translate(-550 230)')]
            for node,row in zip(groups,(0,0,1,2)):
                ink.remove(node);rows[row].append(node)
            frame.set('viewBox','-270 -15 540 350')
            frame.set('data-lines','你好，|我是|ForgetWolf')
            continue

        if code == 'en':
            lines = ('Hello,','ForgetWolf','here.')
            font = E.parse(ROOT/'HersheyScript1.svg').getroot()
            glyphs = {g.get('unicode'):g for g in font.iter(S+'glyph')}
            def count(character):
                return sum(len(set(coordinates(part)))>1 for part in re.findall(r'M[^M]+',glyphs[character].get('d','')))
            line_counts = [sum(count(c) for c in line) for line in lines]
            line_spacing, padding = 1200, 160
        else:
            lines = ('Здравствуйте,','здесь','ФоргетВолк.')
            text = (ROOT/'GOST-font.coffee').read_text(encoding='utf-8')
            glyphs = {char:path for char,path in re.findall(r'^    "([^"\n]+)":\s*\n\s*d: "([^"]*)"',text,re.M)}
            def count(character):
                # The original renderer retains moveto/lineto runs with 2+ points.
                return sum(len(re.findall(r'l\s*-?[\d.]+,',part))>0 for part in re.split(r'm\s*-?[\d.]+,-?[\d.]+',glyphs[character])[1:])
            line_counts = [sum(count(c) for c in line) for line in lines]
            line_spacing, padding = 20, 2
        paths = ink.findall(S+'path')
        assert sum(line_counts) == len(paths), (code,line_counts,len(paths))
        all_points, index = [], 0
        for row,length in enumerate(line_counts):
            selected = paths[index:index+length];index += length
            points = []
            for path in selected:
                coords = coordinates(path.get('d'))
                if code == 'en':
                    advance = float(re.search(r'translate\(([-\d.]+)',path.get('transform')).group(1))
                    coords = [(x+advance,-y) for x,y in coords]
                points.extend(coords)
            left,right = min(x for x,y in points),max(x for x,y in points)
            dx,dy = -(left+right)/2,row*line_spacing
            group = E.SubElement(ink,S+'g',{'transform':f'translate({dx} {dy})'})
            for path in selected: ink.remove(path);group.append(path)
            all_points.extend((x+dx,y+dy) for x,y in points)
        left=min(x for x,y in all_points)-padding;top=min(y for x,y in all_points)-padding
        width=max(x for x,y in all_points)-left+padding;height=max(y for x,y in all_points)-top+padding
        frame.set('viewBox',f'{left} {top} {width} {height}')
        frame.set('data-lines','|'.join(lines))
