"""Check combined timing against the original tracks, including loop resets."""
import bisect
import math
import re
import xml.etree.ElementTree as ET
from generate import ROOT, S, SOURCES, load, seconds

root = ET.parse(ROOT / "carousel/carousel.svg").getroot()
ids = [x.get("id") for x in root.iter() if x.get("id")]
assert len(ids) == len(set(ids))
for node in root.iter():
    for value in node.attrib.values():
        for fragment in re.findall(r"url\(#([^)]*)\)", value):
            assert fragment in ids, fragment

checks = 0
for combined in root:
    language = combined.get("data-language")
    original, first, length = load(language)
    offset = float(combined.get("data-start"))
    assert math.isclose(float(combined.get("data-end")) - offset, length)
    assert [x.get("d") for x in original.iter(S + "path")] == [x.get("d") for x in combined.iter(S + "path")]
    original_nodes = [x for x in original.iter() if x.tag not in (S + "animate", S + "set")]
    combined_nodes = [x for x in combined.iter() if x.tag not in (S + "animate", S + "set")]
    assert len(original_nodes) == len(combined_nodes)
    for source, target in zip(original_nodes, combined_nodes):
        for track in target.findall(S + "animate"):
            attribute = track.get("attributeName")
            events = [x for x in source if x.get("attributeName") == attribute]
            assert events
            values = track.get("values").split(";")
            keys = list(map(float, track.get("keyTimes").split(";")))
            total = seconds(track.get("dur"))
            assert len(keys) == len(values) and keys[0] == 0 and keys[-1] == 1
            assert all(a < b for a, b in zip(keys, keys[1:]))
            assert track.get("repeatCount") == "indefinite"
            # Both sides of every event boundary, plus samples through writing.
            samples = [length * i / 40 for i in range(41)]
            for event in events:
                start = seconds(event.get("begin")) - first
                end = start + seconds(event.get("dur", "0s"))
                samples += [start - 1e-6, start + 1e-6, end - 1e-6, end + 1e-6]
            for local in samples:
                if not 0 <= local < length:
                    continue
                expected = source.get(attribute, "hidden") if track.get("calcMode") == "discrete" else events[0].get("from")
                for event in events:
                    elapsed = local + first - seconds(event.get("begin"))
                    if elapsed < 0:
                        continue
                    if event.tag == S + "set":
                        expected = event.get("to")
                    else:
                        f = min(1, elapsed / seconds(event.get("dur")))
                        expected = float(event.get("from")) * (1-f) + float(event.get("to")) * f
                for cycle in (0, 1, 2):
                    phase = ((offset + local + cycle * total) % total) / total
                    # keyTimes are serialized decimals; allow their rounding
                    # error at an exact boundary (under 30 picoseconds).
                    i = min(len(keys) - 2, max(0, bisect.bisect_right(keys, phase + 1e-12) - 1))
                    if track.get("calcMode") == "discrete":
                        assert values[i] == expected, (language, attribute, local, cycle, values[i], expected, keys)
                    else:
                        f = (phase - keys[i]) / (keys[i + 1] - keys[i])
                        actual = float(values[i]) * (1-f) + float(values[i+1]) * f
                        assert math.isclose(actual, float(expected), abs_tol=2e-6), (language, attribute, local, actual, expected)
                    checks += 1
    print(f"{language}: original paths and animation timing preserved")
print(f"Passed {checks} timeline comparisons across three cycles; IDs and references valid.")
