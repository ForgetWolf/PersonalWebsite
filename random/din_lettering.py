"""Outline the landing's DIN lettering; no browser font loading or CSS."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / ".tools"))
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

S = "{http://www.w3.org/2000/svg}"


def terminal_cell_width(size):
    font = TTFont(ROOT / "random/fonts/IBM-VGA-8x16.woff")
    width = font["hmtx"][font.getBestCmap()[ord("0")]][0] * size / font["head"].unitsPerEm
    font.close()
    return width


def outline_text(root):
    fonts = {
        "din": TTFont(ROOT / "random/fonts/din1451alt.ttf"),
        "terminal": TTFont(ROOT / "random/fonts/IBM-VGA-8x16.woff"),
    }
    metadata = ET.SubElement(root, S + "metadata")
    metadata.text = ("Landing lettering: Alte DIN 1451 Mittelschrift by Peter Wiegel. "
                     "Copyright (c) 2017 Peter Wiegel. SIL Open Font License 1.1. "
                     "Source: https://www.peter-wiegel.de/alteDin1451.html. "
                     "License distributed in random/fonts/Open Font License.txt. "
                     "Output lettering: WebPlus IBM VGA 8x16 by VileR, Oldschool PC Font Pack v2.2, "
                     "https://int10h.org/oldschool-pc-fonts/, CC BY-SA 4.0; converted to SVG outlines. "
                     "License: random/fonts/VGA-LICENSE.txt.")
    for parent in list(root.iter()):
        for text in list(parent.findall(S + "text")):
            value = text.text or ""
            terminal = text.get("font-family") == "VGA Terminal"
            font = fonts["terminal" if terminal else "din"]
            cmap = font.getBestCmap()
            glyphs = font.getGlyphSet()
            units = font["head"].unitsPerEm
            # Preserve the requested fallback if any digit is unavailable.
            if any(ord(character) not in cmap for character in value):
                continue
            names = [cmap[ord(character)] for character in value]
            scale = float(text.get("font-size", "16")) / units
            width = sum(font["hmtx"][name][0] for name in names)
            x, y = float(text.get("x", "0")), float(text.get("y", "0"))
            if text.get("text-anchor") == "middle":
                x -= width * scale / 2
            group = ET.Element(S + "g", {
                "transform": f"translate({x:.6f} {y:.6f}) scale({scale:.9f} {-scale:.9f})",
                "fill": text.get("fill", "CanvasText"), "aria-label": value,
                "data-text": value, "data-font": "IBM VGA 8x16" if terminal else "Alte DIN 1451 Mittelschrift",
            })
            for key in ("id", "opacity"):
                if text.get(key) is not None:
                    group.set(key, text.get(key))
            position = 0
            for name in names:
                pen = SVGPathPen(glyphs)
                glyphs[name].draw(pen)
                ET.SubElement(group, S + "path", {
                    "d": pen.getCommands(), "transform": f"translate({position} 0)",
                })
                position += font["hmtx"][name][0]
            index = list(parent).index(text)
            parent.remove(text)
            parent.insert(index, group)
    for font in fonts.values():
        font.close()
