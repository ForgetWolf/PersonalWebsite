"""SVG-only information toggle prototype; no dragging, scrolling or scripts."""
from copy import deepcopy
import xml.etree.ElementTree as ET
S = "{http://www.w3.org/2000/svg}"
DURATION = ".6875s"


def tween(parent, attribute, closed, opened, tag="animate", **extra):
    for event, start, end in (("infoOpen.click", closed, opened), ("infoClose.click", opened, closed)):
        ET.SubElement(parent, S + tag, {
            "attributeName": attribute, "from": str(start), "to": str(end),
            "begin": event, "dur": DURATION, "fill": "freeze", "calcMode": "spline",
            "keyTimes": "0;1", "keySplines": ".22 .75 .25 1", **extra,
        })


def add_info_toggle(landing, active_page=None):
    layer = ET.SubElement(landing, S + "g", {"id": "info-layer", "display": "none", "opacity": "0"})
    ET.SubElement(layer, S + "set", {
        "attributeName": "display", "to": "inline", "begin": "infoOpen.click",
        "end": "infoClose.click+" + DURATION, "fill": "remove", "restart": "always",
    })
    tween(layer, "opacity", 0, 1)
    nav_anchor = ET.SubElement(layer, S + "g", {"transform": "translate(-436 0)"})
    view = ET.SubElement(nav_anchor, S + "svg", {
        "id": "nav-strip", "x": "95%", "y": "2%", "width": "400", "height": "56",
        "viewBox": "0 0 400 56",
    })
    ET.SubElement(view, S + "rect", {"width": "400", "height": "56", "fill": "Canvas"})
    content = ET.SubElement(view, S + "g", {"transform": "translate(18 0)"})
    tween(content, "transform", "18 0", "0 0", tag="animateTransform", type="translate")
    for label, x in (("About",42),("Blog",130),("Friends",225),("Contact",328)):
        current = label.lower() == active_page
        url = "https://forgetwolf.net/" if current else ("https://blog.forgetwolf.net" if label == "Blog" else "https://forgetwolf.net/" + label.lower())
        item = ET.SubElement(content, S + "a", {"id": "nav-" + label.lower(), "href": url, "aria-label": "Main" if current else label})
        ET.SubElement(item, S + "rect", {"x":str(x-40),"y":"10","width":"80","height":"36","fill":"transparent"})
        ET.SubElement(item, S + "text", {
            "x": str(x), "y": "34", "font-size": "18", "text-anchor": "middle", "fill": "CanvasText",
        }).text = "Main" if current else label
    ET.SubElement(view, S + "line", {
        "x1": "390", "x2": "390", "y1": "16", "y2": "40",
        "stroke": "CanvasText", "stroke-opacity": ".25", "stroke-width": "1",
    })

    button = ET.SubElement(landing, S + "svg", {
        "id": "info-toggle", "x": "90%", "y": "2%", "width": "10%", "height": "56",
        "viewBox": "-50 -50 100 100", "preserveAspectRatio": "xMidYMid meet",
        "role": "group", "aria-label": "Navigation menu toggle",
    })
    glyph = ET.SubElement(button, S + "g")
    for name, offset, direction in (("top",-8,1),("bottom",8,-1)):
        shift = ET.SubElement(glyph, S + "g", {"transform": f"translate(0 {offset})"})
        tween(shift, "transform", f"0 {offset}", "0 0", tag="animateTransform", type="translate")
        bar = ET.SubElement(shift, S + "line", {
            "id": "menu-bar-" + name, "x1": "-12", "x2": "12", "y1": "0", "y2": "0",
            "stroke": "CanvasText", "stroke-width": "4", "stroke-linecap": "round",
        })
        for event, angles in (("infoOpen.click", (0,48,44,45)),
                              ("infoClose.click", (45,-3,1,0))):
            ET.SubElement(bar, S + "animateTransform", {
                "attributeName": "transform", "type": "rotate",
                "values": ";".join(str(angle*direction) for angle in angles),
                "keyTimes": "0;.68;.86;1", "dur": DURATION, "begin": event,
                "fill": "freeze", "calcMode": "spline",
                "keySplines": ".22 .75 .25 1;.35 0 .65 1;.35 0 .65 1",
            })
    middle = ET.SubElement(glyph, S + "line", {
        "id": "menu-bar-middle", "x1": "-12", "x2": "12", "y1": "0", "y2": "0",
        "stroke": "CanvasText", "stroke-width": "4", "stroke-linecap": "round",
    })
    tween(middle, "x1", -12, 0); tween(middle, "x2", 12, 0); tween(middle, "opacity", 1, 0)
    # Two alternating hit regions store open/closed state. During each short
    # transition both are disabled, so rapid taps cannot jump fixed endpoints.
    for name, initial, active, begin, end in (
        ("infoOpen", "inline", "none", "infoOpen.click", "infoClose.click+" + DURATION),
        ("infoClose", "none", "inline", "infoOpen.click+" + DURATION, "infoClose.click"),
    ):
        hit = ET.SubElement(button, S + "rect", {
            "id": name, "x": "-48", "y": "-48", "width": "96", "height": "96",
            "fill": "transparent", "display": initial, "pointer-events": "all",
        })
        # A single bounded interval per target avoids competing indefinite
        # frozen sets across repeated Safari touch cycles. Ending the interval
        # restores the base display value instead of stacking another override.
        ET.SubElement(hit, S + "set", {
            "attributeName": "display", "to": active, "begin": begin, "end": end,
            "fill": "remove", "restart": "always",
        })


def make_info_toggle_fullscreen(parent):
    """Reflow the navigation into a viewport-sized mobile overlay."""
    layer = parent.find(S + "g[@id='info-layer']")
    anchor = layer.find(S + "g")
    anchor.set("transform", "translate(0 -150)")
    nav = anchor.find(S + "svg")
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
    button = parent.find(S + "svg[@id='info-toggle']")
    position = list(parent).index(button)
    parent.remove(button)
    header = ET.Element(S + "svg", dict(button.attrib))
    parent.insert(position, header)
    ET.SubElement(header, S + "set", {
        "attributeName":"display", "to":"none", "begin":"infoOpen.click",
        "end":"infoClose.click+" + DURATION, "fill":"remove", "restart":"always",
    })
    for y in (-8,0,8):
        ET.SubElement(header, S + "line", {"x1":"-12","x2":"12","y1":str(y),"y2":str(y),
            "stroke":"CanvasText","stroke-width":"4","stroke-linecap":"round"})
    ET.SubElement(header, S + "rect", {"id":"infoOpen","x":"-48","y":"-48","width":"96","height":"96","fill":"transparent","pointer-events":"all"})
    close_group = ET.SubElement(parent, S + "g", {"id":"mobile-close-control", "display":"none", "opacity":"0", "transform":"translate(0 18)"})
    ET.SubElement(close_group, S + "set", {
        "attributeName":"display","to":"inline","begin":"infoOpen.click",
        "end":"infoClose.click+" + DURATION,"fill":"remove","restart":"always",
    })
    for animation in layer.findall(S + "animate"):
        if animation.get("attributeName") == "opacity":
            close_group.append(deepcopy(animation))
    close_svg = ET.SubElement(close_group, S + "svg", {
        "x":"45%","y":"75%","width":"10%","height":"56","viewBox":"-50 -50 100 100",
        "preserveAspectRatio":"xMidYMid meet","aria-label":"Close navigation",
    })
    for y1,y2 in ((-9,9),(9,-9)):
        ET.SubElement(close_svg,S+'line',{"x1":"-9","x2":"9","y1":str(y1),"y2":str(y2),"stroke":"CanvasText","stroke-width":"4","stroke-linecap":"round"})
    hit = ET.SubElement(close_svg,S+'rect',{"id":"infoClose","x":"-48","y":"-48","width":"96","height":"96","fill":"transparent","display":"none","pointer-events":"all"})
    ET.SubElement(hit,S+'set',{"attributeName":"display","to":"inline","begin":"infoOpen.click+" + DURATION,"end":"infoClose.click","fill":"remove","restart":"always"})
