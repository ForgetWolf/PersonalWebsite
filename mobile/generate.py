"""Independent mobile SVGs, preserving the desktop greeting and toggle timing."""
from pathlib import Path
import xml.etree.ElementTree as ET
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'random'))
from brainfuck_animation import relayout_output
from din_lettering import outline_text
from info_toggle import make_info_toggle_fullscreen
from welcome_layout import reflow_welcomes

ROOT = Path(__file__).resolve().parent.parent
S = "{http://www.w3.org/2000/svg}"
ET.register_namespace("", S[1:-1])

for source in sorted((ROOT / "random").glob("*.svg")):
    root = ET.parse(source).getroot()
    root.set("data-layout", "mobile")
    reflow_welcomes(root)
    make_info_toggle_fullscreen(root.find(S + "g[@id='landing']"))
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
