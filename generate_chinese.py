"""Compose supplied Chinese stroke SVGs and original Hershey Latin paths.

Only SVG/SMIL is served. All timing and assembly happens during authoring.
"""
from pathlib import Path
import copy
import json
import math
import re
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parent
cfg = json.loads((ROOT / 'languages/zh.json').read_text(encoding='utf-8'))
URI = 'http://www.w3.org/2000/svg'
E.register_namespace('', URI)
def tag(name):
    return '{' + URI + '}' + name

outer = E.Element(tag('svg'), {'width': '100%', 'height': '95vh',
    'role': 'img', 'aria-label': cfg['text']})
frame = E.SubElement(outer, tag('svg'), {'x': '10%', 'y': '35%',
    'width': '80%', 'height': '30%', 'viewBox': '-20 -10 810 130',
    'preserveAspectRatio': 'xMidYMid meet'})
group = E.SubElement(frame, tag('g'), {'opacity': '0'})
fade_start = cfg['total_seconds'] - cfg['fade_out']
write_end = fade_start - cfg['hold']
for start, duration, a, b in [(cfg['initial_wait'], cfg['fade_in'], '0', '1'),
                              (fade_start, cfg['fade_out'], '1', '0')]:
    E.SubElement(group, tag('animate'), {'attributeName': 'opacity', 'from': a,
        'to': b, 'begin': f'{start:.5f}s', 'dur': f'{duration:.5f}s', 'fill': 'freeze'})


def chinese(filename, prefix, transform, start, duration):
    source = E.parse(ROOT / 'lettering' / filename).getroot()
    if source.get('data-timeline-start'):
        first=float(source.get('data-timeline-start'))
        end=float(source.get('data-timeline-end'))
    else:
        animations = [a for a in source.iter(tag('animate')) if a.get('to') == '0']
        first = min(float(a.get('begin')[:-1]) for a in animations)
        end = max(float(a.get('begin')[:-1]) + float(a.get('dur')[:-1]) for a in animations)
    factor = duration / (end - first)
    wrapper = E.SubElement(group, tag('g'), {'transform': transform})
    for child in source:
        if child.tag in [tag('title'), tag('desc')]:
            continue
        wrapper.append(copy.deepcopy(child))
    for el in wrapper.iter():
        if el.get('id'):
            el.set('id', prefix + el.get('id'))
        for attr in ['mask', 'clip-path']:
            if el.get(attr):
                el.set(attr, el.get(attr).replace('url(#', 'url(#' + prefix))
        if el.get('fill', '').lower() in ('#000', '#000000', 'black'):
            el.set('fill', 'CanvasText')
    if source.get('data-timeline-start'):
        for el in wrapper.iter():
            if el.tag in [tag('animate'),tag('set')] and el.get('begin'):
                el.set('begin',f"{start+(float(el.get('begin')[:-1])-first)*factor:.7f}s")
                if el.get('dur'):
                    el.set('dur',f"{float(el.get('dur')[:-1])*factor:.7f}s")
        return
    for mask in wrapper.iter(tag('mask')):
        path = mask.find(tag('path'))
        reveal = next(a for a in path.findall(tag('animate')) if a.get('to') == '0')
        begin = start + (float(reveal.get('begin')[:-1]) - first) * factor
        dur = float(reveal.get('dur')[:-1]) * factor
        for a in list(path):
            path.remove(a)
        # Explicitly hide inactive mask paths, including rounded initial caps.
        path.set('stroke-dashoffset', '100')
        path.set('visibility', 'hidden')
        E.SubElement(path, tag('set'), {'attributeName': 'visibility', 'to': 'visible',
            'begin': f'{begin:.5f}s', 'fill': 'freeze'})
        reveal.set('begin', f'{begin:.5f}s')
        reveal.set('dur', f'{dur:.5f}s')
        path.append(reveal)


t = cfg['initial_wait']
chinese('ni.svg', 'zh_ni_', 'translate(0 -2) scale(.25)', t, cfg['ni_seconds'])
t += cfg['ni_seconds'] + cfg['section_gap']
chinese('comma.svg', 'zh_comma_', 'translate(150 55) scale(.15)', t, cfg['comma_seconds'])
t += cfg['comma_seconds'] + cfg['section_gap']
chinese('wo.svg', 'zh_wo_', 'translate(175 -2) scale(.23)', t, cfg['wo_seconds'])
t += cfg['wo_seconds'] + cfg['section_gap']

font = E.parse(ROOT / 'HersheyScript1.svg').getroot()
metadata = E.SubElement(outer, tag('metadata'))
metadata.text = ('Chinese lettering traced from user handwriting, with gently smoothed contours.\n'
                 + font.find(tag('metadata')).text)
glyphs = {g.get('unicode'): g for g in font.iter(tag('glyph'))}
strokes = []
x = 0.0
for char in 'ForgetWolf':
    glyph = glyphs[char]
    for d in re.findall(r'M[^M]+', glyph.get('d', '')):
        numbers = list(map(float, re.findall(r'-?\d+(?:\.\d+)?', d)))
        coords = list(zip(numbers[::2], numbers[1::2]))
        length = sum(math.dist(a, b) for a, b in zip(coords, coords[1:]))
        if length > 0:
            strokes.append((x, d.strip(), length))
    x += float(glyph.get('horiz-adv-x', '378'))
latin = E.SubElement(group, tag('g'), {'transform': 'translate(310 65) scale(.1 -.1)',
    'fill': 'none', 'stroke': 'CanvasText', 'stroke-width': '17',
    'stroke-linecap': 'round', 'stroke-linejoin': 'round'})
pause = .025
draw_seconds = write_end - t - pause * (len(strokes) - 1)
if draw_seconds <= 0:
    raise ValueError('Insufficient time for Latin strokes')
speed = sum(s[2] for s in strokes) / draw_seconds
for x, d, length in strokes:
    dur = length / speed
    path = E.SubElement(latin, tag('path'), {'transform': f'translate({x:g} 0)',
        'd': d, 'pathLength': '1', 'stroke-dasharray': '1 1', 'stroke-dashoffset': '1',
        'visibility': 'hidden'})
    E.SubElement(path, tag('set'), {'attributeName': 'visibility', 'to': 'visible',
        'begin': f'{t:.5f}s', 'fill': 'freeze'})
    E.SubElement(path, tag('animate'), {'attributeName': 'stroke-dashoffset', 'from': '1',
        'to': '0', 'begin': f'{t:.5f}s', 'dur': f'{dur:.5f}s', 'fill': 'freeze', 'calcMode': 'linear'})
    t += dur + pause
result = E.tostring(outer, encoding='unicode')
(ROOT / 'chinese.hpp').write_text(
    '// Generated by generate_chinese.py; Chinese source SVGs in lettering/.\n#pragma once\n'
    'inline constexpr const char* chinese_svg = u8R"CHINESE(\n' + result + '\n)CHINESE";\n',
    encoding='utf-8')
brush_count = len(outer.findall('.//' + tag('mask')))
print(f'Chinese: {brush_count} brush pen routes + {len(strokes)} Latin strokes; writing ends {t-pause:.3f}s; '
      f'fade ends {cfg["total_seconds"]:.3f}s')
