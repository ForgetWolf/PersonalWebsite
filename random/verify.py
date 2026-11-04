"""Validate every one-shot permutation, original geometry and finish events."""
import itertools
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "carousel"))
from generate import load, S, seconds

for order in itertools.permutations(("en", "ru", "zh")):
    file = ROOT / "random" / ("-".join(order) + ".svg")
    text = file.read_text(encoding="utf-8")
    root = ET.fromstring(text)
    assert not any(x in text for x in ("<script", "<style", "style=", "<title"))
    assert {a.get('href') for a in root.iter(S+'a')} == {'https://forgetwolf.net/about','https://forgetwolf.net/friends','https://forgetwolf.net/contact','https://blog.forgetwolf.net'}
    assert root.get("data-order").split(",") == list(order)
    languages = [x for x in root.iter(S + "svg") if x.get("data-language")]
    assert [x.get("data-language") for x in languages] == list(order)
    previous = 0.0
    for svg in languages:
        original, _, _ = load(svg.get("data-language"))
        assert [x.get("d") for x in svg.iter(S + "path")] == [x.get("d") for x in original.iter(S + "path")]
        assert float(svg.get("data-start")) == previous
        previous = float(svg.get("data-end"))
        for animation in svg.iter(S + "animate"):
            assert animation.get("repeatCount") == "1"
            assert animation.get("fill") == "freeze"
            assert abs(seconds(animation.get("dur")) - float(root.get("data-duration"))) < 1e-8
    switches = [x for x in root.iter(S + "set") if 'randomScene.click' in x.get('begin','')]
    assert len(switches) == 2
    assert switches[0].get("begin") == switches[1].get("begin")
    assert "randomScene.click" in switches[0].get("begin")
    assert "randomPage.click" in switches[0].get("begin")
    assert abs(seconds(switches[0].get("begin").split(";")[0]) - previous) < 1e-8
    assert (switches[0].get("attributeName"), switches[0].get("to")) == ("display", "none")
    assert (switches[1].get("attributeName"), switches[1].get("to")) == ("display", "inline")
    assert "ForgetWolf" in text
    assert root.find(".//" + S + "foreignObject") is None
    landing = root.find(S + "g[@id='landing']")
    assert landing.get("display") == "none"
    assert root.find(".//" + S + "rect[@id='welcome-background']").get("fill") == "Canvas"
    assert landing.find(".//" + S + "g[@data-text='ForgetWolf']") is not None
print("All six permutations verified, with four navigation links and no JS/CSS.")
