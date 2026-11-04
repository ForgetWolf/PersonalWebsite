"""Build all six single-play orders, with native SMIL click-to-finish."""
from itertools import permutations
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "carousel"))
from generate import S, load, repeat, number
from din_lettering import outline_text
from brainfuck_animation import add_animation, relayout_output
from info_toggle import add_info_toggle


def build(order):
    languages = [(language, *load(language)) for language in order]
    total = sum(length for _, _, _, length in languages)
    root = ET.Element(S + "svg", {
        "id": "randomScene", "width": "100%", "height": "97vh",
        "data-order": ",".join(order), "data-duration": number(total),
    })
    # Hit area has no link, title, cursor style or other hover affordance.
    ET.SubElement(root, S + "rect", {"width": "100%", "height": "100%", "fill": "transparent"})
    greetings = ET.SubElement(root, S + "g")
    finish = number(total) + "s;randomScene.click;randomPage.click"
    ET.SubElement(greetings, S + "set", {
        "attributeName": "display", "to": "none", "begin": finish, "fill": "freeze",
    })
    ET.SubElement(greetings, S + "rect", {
        "id": "welcome-background", "width": "100%", "height": "100%",
        "fill": "Canvas",
    })
    offset = 0.0
    for language, svg, first, length in languages:
        repeat(svg, first, offset, total)
        for animation in svg.iter(S + "animate"):
            animation.set("repeatCount", "1")
            animation.set("fill", "freeze")
        svg.set("height", "100%")
        svg.set("data-language", language)
        svg.set("data-start", number(offset))
        svg.set("data-end", number(offset + length))
        greetings.append(svg)
        offset += length
    # display:none cannot be overridden by child visibility animations; an
    # inherited visibility:hidden could leak the animated cell numbers.
    result = ET.SubElement(root, S + "g", {"id": "landing", "display": "none"})
    ET.SubElement(result, S + "set", {
        "id": "landingEnter", "attributeName": "display", "to": "inline",
        "begin": finish, "fill": "freeze", "restart": "never",
    })
    # Keep the event animation in SVG namespace. Under text/html, children of
    # foreignObject are parsed as HTML, so a set placed there is not SMIL.
    heading = ET.SubElement(result, S + "svg", {
        "id": "landing-heading", "x": "5%", "y": "0", "width": "90%", "height": "76.8",
        "viewBox": "0 0 360 76.8", "preserveAspectRatio": "xMidYMin meet",
    })
    text = ET.SubElement(heading, S + "text", {
        "x": "180", "y": "57.6", "text-anchor": "middle", "fill": "CanvasText", "font-size": "57.6",
        "font-family": "Times New Roman, serif",
    })
    text.text = "ForgetWolf"
    # One shared viewport scales output, spacing and tape together. Apply any
    # offset to a SVG 1.1 g, not to a nested svg (Safari compatibility).
    stage_offset = ET.SubElement(result, S + "g", {"transform": "translate(0 -131.4)"})
    stage = ET.SubElement(stage_offset, S + "svg", {
        "id": "terminal-stage", "x": "5%", "y": "50%", "width": "90%", "height": "262.8",
        "viewBox": "0 0 600 262.8", "preserveAspectRatio": "xMidYMid meet",
    })
    tape = ET.SubElement(stage, S + "svg", {
        "id": "brainfuck-tape", "x": "0", "y": "187.2",
        "width": "600", "height": "75.6", "viewBox": "0 0 1000 126",
        "preserveAspectRatio": "xMidYMid meet", "role": "img",
        "aria-label": "Brainfuck memory: 15 zero-initialized cells, pointer at cell 0",
    })
    ET.SubElement(tape, S + "rect", {
        "x": "1", "y": "31", "width": "998", "height": "64",
        "fill": "none", "stroke": "CanvasText", "stroke-width": "2",
    })
    for index in range(15):
        x = 1000 * (index + .5) / 15
        if index:
            ET.SubElement(tape, S + "line", {
                "x1": number(index * 1000 / 15), "x2": number(index * 1000 / 15),
                "y1": "31", "y2": "95", "stroke": "CanvasText", "stroke-width": "2",
            })
        label = ET.SubElement(tape, S + "text", {
            "x": number(x), "y": "16", "text-anchor": "middle",
            "font-family": "monospace", "font-size": "14",
            "fill": "CanvasText", "opacity": ".55",
        })
        label.text = str(index)
        cell = ET.SubElement(tape, S + "text", {
            "id": f"bf-cell-{index}", "x": number(x), "y": "72",
            "text-anchor": "middle", "font-family": "monospace",
            "font-size": "24", "font-weight": "bold", "fill": "CanvasText",
        })
        cell.text = "0"
    ET.SubElement(tape, S + "path", {
        "id": "bf-pointer", "d": "M33.333 104 L28.333 113 L38.333 113 Z",
        "fill": "CanvasText",
    })
    add_animation(result, tape)
    output = result.find(S + "svg[@id='brainfuck-output']")
    result.remove(output)
    output.attrib.pop("transform", None)
    output.attrib.update({"x": "0", "y": "0", "width": "600", "height": "127.2"})
    stage.append(output)
    output_height = relayout_output(output)
    tape.set('y', str(output_height+45))
    total_height = output_height+45+75.6
    stage.set('height',str(total_height));stage.set('viewBox',f'0 0 600 {total_height}')
    stage_offset.set('transform',f'translate(0 {-total_height/2})')
    add_info_toggle(result)
    outline_text(result)
    ET.ElementTree(root).write(ROOT / "random" / ("-".join(order) + ".svg"), encoding="utf-8")


if __name__ == "__main__":
    for order in permutations(("en", "ru", "zh")):
        build(order)
    print("Built six single-play SVG orders with native click/timeout transitions.")
