"""Offline checks and snapshots of the post-welcome output animation."""
import bisect
import copy
import io
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / ".tools"))
import resvg_py
from PIL import Image, ImageDraw
from brainfuck_animation import preset_trace, S

changes, moves, outputs, duration = preset_trace()
assert outputs[-1][1] == (ROOT / "random/message.md").read_text().strip()
assert all(0 <= value <= 255 for events in changes for _, value in events)
assert all(0 <= value < 15 for _, value in moves)
root = ET.parse(ROOT / "random/en-ru-zh.svg").getroot()
landing = root.find(S + "g[@id='landing']")
assert landing.find(S + "set").get("restart") == "never"
stage = landing.find(".//" + S + "svg[@id='terminal-stage']")
tracks = list(stage.iter(S + "animate")) + list(stage.iter(S + "animateTransform"))
for animation in tracks:
    assert animation.get("begin") == "landingEnter.begin"
    assert animation.get("restart") == "never"
    keys = list(map(float, animation.get("keyTimes").split(";")))
    assert keys[0] == 0 and keys[-1] == 1
    assert all(a < b for a,b in zip(keys,keys[1:]))
    assert len(keys) == len(animation.get("values").split(";"))

times = [.25, 8, 25, duration]
sheet = Image.new("RGB", (1000, 440*len(times)), "#111")
draw = ImageDraw.Draw(sheet)
for row,time in enumerate(times):
    frame = copy.deepcopy(root)
    frame.set("width", "1000"); frame.set("height", "410")
    for node in list(frame):
        if node.get('id') != 'landing': frame.remove(node)
    active = frame.find(S+"g[@id='landing']")
    active.set('display','inline')
    for node in list(active):
        if node.get('id') in ('info-layer','info-toggle'): active.remove(node)
    for parent in frame.iter():
        for animation in list(parent):
            if animation.tag == S + "set":
                pass # Welcome/menu sets do not participate in this snapshot.
                parent.remove(animation)
            elif animation.tag in (S + "animate", S + "animateTransform"):
                if animation.get("begin") == "landingEnter.begin":
                    keys = list(map(float,animation.get("keyTimes").split(";")))
                    values = animation.get("values").split(";")
                    track_duration = float(animation.get("dur")[:-1])
                    phase = (time % track_duration) / track_duration if animation.get("repeatCount") == "indefinite" else min(1,time/track_duration)
                    i = min(len(keys)-1, bisect.bisect_right(keys, phase+1e-10)-1)
                    value = values[i]
                    if animation.tag == S + "animateTransform": value = "translate("+value+")"
                    parent.set(animation.get("attributeName"),value)
                parent.remove(animation)
    markup = ET.tostring(frame,encoding="unicode").replace("CanvasText","#fff")
    png = resvg_py.svg_to_bytes(svg_string=markup,background="#111")
    sheet.paste(Image.open(io.BytesIO(png)).convert("RGB"),(0,row*440+25))
    draw.text((12,row*440+6),f'{time:.2f}s after entry',fill="white")
sheet.save(ROOT / "random/output-proof.png")
print(f'Validated {len(tracks)} SVG tracks; saved output-proof.png')
