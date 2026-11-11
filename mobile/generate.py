"""Independent mobile SVGs, preserving the desktop greeting and toggle timing."""
from pathlib import Path
import xml.etree.ElementTree as ET
import re
import copy
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'random'))
from brainfuck_animation import relayout_output
from din_lettering import outline_text
from welcome_layout import reflow_welcomes

ROOT = Path(__file__).resolve().parent.parent
S = "{http://www.w3.org/2000/svg}"
ET.register_namespace("", S[1:-1])

for source in sorted((ROOT / "random").glob("*.svg")):
    root = ET.parse(source).getroot()
    root.set("data-layout", "mobile")
    reflow_welcomes(root)
    nav = next(node for node in root.iter(S + "svg") if node.get("id") == "nav-strip")
    anchor = next(node for node in root.iter() if nav in list(node))
    anchor.set("transform", "translate(0 -150)")
    layer = next(node for node in root.iter() if node.get("id") == "info-layer")
    # Full-screen Canvas is independent of the scaled/centered menu contents.
    layer.insert(0, ET.Element(S + "rect", {"width":"100%", "height":"100%", "fill":"Canvas"}))
    nav.attrib.update({"x":"5%", "y":"50%", "width": "90%", "height": "300", "viewBox": "0 0 400 300"})
    background = nav.find(S + "rect")
    nav.remove(background)
    content = nav.find(S + "g")
    content.set("transform", "translate(0 -18)")
    for animation in content.findall(S + "animateTransform"):
        opening = animation.get("begin") == "infoOpen.click"
        animation.set("from", "0 -18" if opening else "0 0")
        animation.set("to", "0 0" if opening else "0 -18")
    for index, (name, old_x) in enumerate((("about",42),("blog",130),("friends",225),("contact",328))):
        item = content.find(S + f"a[@id='nav-{name}']")
        item.set("transform", f"translate({200-old_x*1.4} {70+index*56-34*1.4}) scale(1.4)")
    for line in list(nav.findall(S + "line")):
        nav.remove(line)
    # Separate static hamburger and static X. Only the X's opacity follows
    # the menu; no glyph morphing or positional animation on mobile.
    button = next(node for node in root.iter() if node.get("id") == "info-toggle")
    parent = next(node for node in root.iter() if button in list(node))
    position = list(parent).index(button)
    parent.remove(button)
    header = ET.Element(S + "svg", dict(button.attrib))
    parent.insert(position, header)
    ET.SubElement(header, S + "set", {
        "attributeName":"display", "to":"none", "begin":"infoOpen.click",
        "end":"infoClose.click+.6875s", "fill":"remove", "restart":"always",
    })
    for y in (-8,0,8):
        ET.SubElement(header, S + "line", {"x1":"-12","x2":"12","y1":str(y),"y2":str(y),
            "stroke":"CanvasText","stroke-width":"4","stroke-linecap":"round"})
    ET.SubElement(header, S + "rect", {"id":"infoOpen","x":"-48","y":"-48","width":"96","height":"96","fill":"transparent","pointer-events":"all"})
    close_group = ET.SubElement(parent, S + "g", {"id":"mobile-close-control", "display":"none", "opacity":"0", "transform":"translate(0 18)"})
    ET.SubElement(close_group, S + "set", {
        "attributeName":"display","to":"inline","begin":"infoOpen.click",
        "end":"infoClose.click+.6875s","fill":"remove","restart":"always",
    })
    for animation in layer.findall(S + "animate"):
        if animation.get("attributeName") == "opacity": close_group.append(copy.deepcopy(animation))
    close_svg = ET.SubElement(close_group, S + "svg", {
        "x":"45%","y":"75%","width":"10%","height":"56","viewBox":"-50 -50 100 100",
        "preserveAspectRatio":"xMidYMid meet","aria-label":"Close navigation",
    })
    for y1,y2 in ((-9,9),(9,-9)):
        ET.SubElement(close_svg,S+'line',{"x1":"-9","x2":"9","y1":str(y1),"y2":str(y2),"stroke":"CanvasText","stroke-width":"4","stroke-linecap":"round"})
    hit = ET.SubElement(close_svg,S+'rect',{"id":"infoClose","x":"-48","y":"-48","width":"96","height":"96","fill":"transparent","display":"none","pointer-events":"all"})
    ET.SubElement(hit,S+'set',{"attributeName":"display","to":"inline","begin":"infoOpen.click+.6875s","end":"infoClose.click","fill":"remove","restart":"always"})
    # Larger terminal type and row spacing, with two extra 1.2x-scaled blocks
    # of vertical room. Geometry is adjusted offline, never through JS/CSS.
    box = next(node for node in root.iter() if node.get("id") == "brainfuck-output")
    output_height = relayout_output(box, font_size=20, line_height=24)
    outline_text(box)
    stage = next(node for node in root.iter() if node.get("id") == "terminal-stage")
    total_height = output_height+45+75.6
    stage.set('height',str(total_height));stage.set('viewBox',f'0 0 600 {total_height}')
    stage_parent = next(node for node in root.iter() if stage in list(node))
    stage_parent.set('transform',f'translate(0 {-total_height/2})')
    tape = next(node for node in stage.iter() if node.get('id') == 'brainfuck-tape')
    tape.set('y',str(output_height+45))
    ET.ElementTree(root).write(ROOT / "mobile" / source.name, encoding="utf-8")
print("Built six mobile pages with larger output and full-screen navigation.")
