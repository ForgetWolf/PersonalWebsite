"""Build an independent repeating SVG; never modify the standalone languages."""
from pathlib import Path
from collections import defaultdict
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
NS = "http://www.w3.org/2000/svg"
ET.register_namespace("", NS)
S = "{" + NS + "}"
SOURCES = {"en": "greeting.hpp", "ru": "russian.hpp", "zh": "chinese.hpp"}


def seconds(value):
    assert value.endswith("s"), value
    return float(value[:-1])


def number(value):
    return f"{value:.12f}".rstrip("0").rstrip(".") or "0"


def load(language):
    source = (ROOT / SOURCES[language]).read_text(encoding="utf-8")
    svg = ET.fromstring(source[source.index("<svg"):source.rindex("</svg>") + 6])
    animations = [x for x in svg.iter() if x.tag in (S + "animate", S + "set")]
    first = min(seconds(x.attrib["begin"]) for x in animations)
    last = max(seconds(x.attrib["begin"]) + seconds(x.get("dur", "0s")) for x in animations)
    ids = {x.get("id"): f"carousel_{language}_{x.get('id')}" for x in svg.iter() if x.get("id")}
    for node in svg.iter():
        for key, value in list(node.attrib.items()):
            if key == "id":
                node.set(key, ids[value])
            else:
                for old, new in ids.items():
                    value = value.replace(f"url(#{old})", f"url(#{new})")
                    if value == "#" + old:
                        value = "#" + new
                node.set(key, value)
    return svg, first, last - first


def repeat(svg, first, offset, total):
    for parent in list(svg.iter()):
        tracks = defaultdict(list)
        for child in list(parent):
            if child.tag in (S + "animate", S + "set"):
                tracks[child.attrib["attributeName"]].append(child)
        for attribute, events in tracks.items():
            events.sort(key=lambda x: seconds(x.attrib["begin"]))
            discrete = events[0].tag == S + "set"
            assert all((x.tag == S + "set") == discrete for x in events)
            initial = parent.get(attribute, "hidden") if discrete else events[0].attrib["from"]
            points = [(0.0, initial)]
            for event in events:
                start = seconds(event.attrib["begin"]) - first + offset
                if discrete:
                    points.append((start, event.attrib["to"]))
                else:
                    assert event.get("calcMode", "linear") == "linear"
                    points.extend([(start, event.attrib["from"]),
                                   (start + seconds(event.attrib["dur"]), event.attrib["to"])])
                parent.remove(event)
            points.append((total, points[-1][1]))
            merged = []
            for time, value in points:
                if merged and abs(merged[-1][0] - time) < 1e-9:
                    merged[-1] = (time, value)
                else:
                    merged.append((time, value))
            ET.SubElement(parent, S + "animate", {
                "attributeName": attribute,
                "values": ";".join(value for _, value in merged),
                "keyTimes": ";".join(number(time / total) for time, _ in merged),
                "dur": number(total) + "s", "begin": "0s",
                "calcMode": "discrete" if discrete else "linear",
                "repeatCount": "indefinite",
            })


def main():
    order = [line.split("#", 1)[0].strip() for line in (ROOT / "carousel/order.txt").read_text().splitlines()]
    order = [line for line in order if line]
    assert len(order) == 3 and set(order) == set(SOURCES), "List en, ru, zh exactly once"
    languages = [(language, *load(language)) for language in order]
    total = sum(length for _, _, _, length in languages)
    result = ET.Element(S + "svg", {"width": "100%", "height": "95vh", "role": "img",
                                     "aria-label": "ForgetWolf greetings in English, Russian and Chinese"})
    offset = 0.0
    for language, svg, first, length in languages:
        repeat(svg, first, offset, total)
        svg.set("height", "100%")
        svg.set("data-language", language)
        svg.set("data-start", number(offset))
        svg.set("data-end", number(offset + length))
        result.append(svg)
        offset += length
    output = ROOT / "carousel/carousel.svg"
    ET.ElementTree(result).write(output, encoding="utf-8", xml_declaration=False)
    print(f"Built {output}: {' -> '.join(order)}, cycle {total:.6f}s")


if __name__ == "__main__":
    main()
