"""Render offline timeline samples without a browser or page scripts."""
import bisect
import copy
import io
import math
import re
import sys
import xml.etree.ElementTree as ET
from generate import ROOT, S, seconds
sys.path.insert(0, str(ROOT / ".tools"))
import resvg_py
from PIL import Image, ImageDraw

root = ET.parse(ROOT / "carousel/carousel.svg").getroot()
root.set("width", "1000")
root.set("height", "220")
times = [8.0, 16.96328, 25.92656, 34.88984]
sheet = Image.new("RGB", (1000, 245 * len(times)), "#111")
draw = ImageDraw.Draw(sheet)
for row, time in enumerate(times):
    frame = copy.deepcopy(root)
    for node in frame.iter():
        for animation in list(node.findall(S + "animate")):
            phase = (time % seconds(animation.get("dur"))) / seconds(animation.get("dur"))
            keys = list(map(float, animation.get("keyTimes").split(";")))
            values = animation.get("values").split(";")
            i = min(len(keys)-2, bisect.bisect_right(keys, phase)-1)
            if animation.get("calcMode") == "discrete":
                value = values[i]
            else:
                fraction = (phase-keys[i])/(keys[i+1]-keys[i])
                value = str(float(values[i])*(1-fraction)+float(values[i+1])*fraction)
            node.set(animation.get("attributeName"), value)
            node.remove(animation)
        if node.get("pathLength"):
            assert set(re.findall(r"[A-Za-z]", node.get("d"))) <= {"M", "L", "e"}
            numbers = list(map(float, re.findall(r"-?\d+(?:\.\d+)?(?:e[+-]?\d+)?", node.get("d"))))
            points = list(zip(numbers[::2], numbers[1::2]))
            scale = sum(math.dist(a,b) for a,b in zip(points, points[1:])) / float(node.get("pathLength"))
            node.set("stroke-dasharray", " ".join(str(float(n)*scale) for n in node.get("stroke-dasharray").split()))
            node.set("stroke-dashoffset", str(float(node.get("stroke-dashoffset"))*scale))
            node.attrib.pop("pathLength")
    markup = ET.tostring(frame, encoding="unicode").replace("CanvasText", "#fff")
    png = resvg_py.svg_to_bytes(svg_string=markup, background="#111")
    sheet.paste(Image.open(io.BytesIO(png)).convert("RGB"), (0, row*245+25))
    draw.text((12,row*245+7), f"{time:.3f}s", fill="white")
sheet.save(ROOT / "carousel/preview.png")
print("Saved carousel/preview.png")
