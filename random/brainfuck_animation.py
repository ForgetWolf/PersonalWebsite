"""Build a static SVG visualization from an offline Brainfuck execution."""
from pathlib import Path
import xml.etree.ElementTree as ET
from din_lettering import terminal_cell_width

S = "{http://www.w3.org/2000/svg}"
ROOT = Path(__file__).resolve().parent.parent
PLAYBACK_SPEED = 1.5
OUTPUT_FONT_SIZE = 15
OUTPUT_LINE_HEIGHT = 18
OUTPUT_ROWS = 5


def preset_trace():
    """Execute the supplied program offline; sample real memory states for SVG."""
    code = ''.join(c for c in (ROOT / 'random/message.bf').read_text(encoding='utf-8-sig') if c in '+-<>[].,')
    stack, jumps = [], {}
    for i, c in enumerate(code):
        if c == '[': stack.append(i)
        elif c == ']':
            start = stack.pop(); jumps[start] = i; jumps[i] = start
    if stack: raise ValueError('Unmatched brackets')
    memory, pointer, pc, steps = {}, 0, 0, 0
    changes = [[(0.0, 0)] for _ in range(15)]
    moves, output, origins = [(0.0, 0)], [(0.0, '')], [(0.0, 0)]
    pending, text, clock = [], '', .25
    def snapshot():
        # Keep each 15-cell page still; only change pages at its boundary.
        origin = (pointer // 15) * 15
        return origin, pointer-origin, tuple(memory.get(origin+i, 0) for i in range(15))
    def append(events, t, value):
        if events[-1][1] != value: events.append((round(t, 6), value))
    while pc < len(code):
        c = code[pc]; value = memory.get(pointer, 0)
        if c == '+': memory[pointer] = (value+1) % 256
        elif c == '-': memory[pointer] = (value-1) % 256
        elif c == '>': pointer += 1
        elif c == '<':
            pointer -= 1
            if pointer < 0: raise ValueError('Negative memory address')
        elif c == '[' and value == 0: pc = jumps[pc]
        elif c == ']' and value != 0: pc = jumps[pc]
        elif c == ',': raise ValueError('Interactive input is unsupported')
        if steps % 64 == 0: pending.append(snapshot())
        if c == '.':
            pending.append(snapshot())
            samples = [pending[0], pending[len(pending)//2], pending[-1]]
            for n, (origin, position, cells) in enumerate(samples):
                t = clock + (n+1)*.03
                append(origins, t, origin); append(moves, t, position)
                for index, cell in enumerate(cells): append(changes[index], t, cell)
            clock = round(clock + .09, 6)
            text += chr(value); output.append((clock, text)); pending = []
        steps += 1; pc += 1
        if steps > 10000000: raise ValueError('Instruction budget exceeded')
    preset_trace.origins = origins
    preset_trace.message = text
    expected = (ROOT / 'source/main.md').read_text(encoding='utf-8-sig').split('---', 2)[2].strip()
    if text.strip() != expected.strip():
        raise ValueError('Brainfuck output differs from source/main.md: ' + repr(text))
    (ROOT / 'random/message.md').write_text(text, encoding='utf-8')
    print(f'Brainfuck verified: {len(text)} characters, {steps} instructions, {clock:.2f}s playback')
    return changes, moves, output, clock + .25


def visible_lines(value):
    # Plan word breaks against the full message so a partially typed word
    # never jumps to the next line halfway through its appearance.
    message = preset_trace.message
    lines = ['']
    for index, character in enumerate(value):
        if character == '\n': lines.append(''); continue
        if not character.isspace() and (index == 0 or message[index-1].isspace()):
            end = index
            while end < len(message) and not message[end].isspace(): end += 1
            if lines[-1] and len(lines[-1]) + end-index > 38: lines.append('')
        if len(lines[-1]) >= 38: lines.append('')
        lines[-1] += character
    return lines[-OUTPUT_ROWS:]


def track(parent, name, events, duration, mode="discrete", tag="animate", **extra):
    ET.SubElement(parent, S + tag, {
        "attributeName": name,
        "values": ";".join(str(value) for _, value in events),
        "keyTimes": ";".join(f"{time/duration:.10f}" for time, _ in events),
        "dur": f"{duration}s", "begin": "landingEnter.begin",
        "restart": "never", "fill": "freeze", "calcMode": mode, **extra,
    })


def states(parent, events, duration, text_attributes):
    for value in dict.fromkeys(value for _, value in events):
        if value == "":
            continue
        visible = [(time, "visible" if current == value else "hidden") for time, current in events]
        # Keep only transitions; each distinct number is drawn once.
        compact = [visible[0]]
        compact.extend(item for previous, item in zip(visible, visible[1:]) if item[1] != previous[1])
        compact.append((duration, compact[-1][1]))
        group = ET.SubElement(parent, S + "g", {"visibility": compact[0][1], "data-state": str(value)})
        ET.SubElement(group, S + "text", text_attributes).text = str(value)
        track(group, "visibility", compact, duration)


def add_animation(landing, tape):
    changes, moves, output, duration = preset_trace()
    message = output[-1][1]
    landing.set("data-output", message)
    landing.set("data-output-duration", str(duration))
    box = ET.SubElement(landing, S + "svg", {
        "id": "brainfuck-output", "x": "5%", "y": "22%", "width": "90%",
        "height": "127.2", "viewBox": "0 0 500 106",
        "transform": "translate(0 -24)", "preserveAspectRatio": "xMidYMid meet",
        "role": "img", "aria-label": "Brainfuck output: " + message,
        "data-rows": str(OUTPUT_ROWS), "data-line-height": str(OUTPUT_LINE_HEIGHT),
    })
    ET.SubElement(box, S + "rect", {
        "x": "1", "y": "1", "width": "498", "height": "104", "rx": "0",
        "fill": "#808080", "fill-opacity": "0.18", "stroke": "none",
    })
    ET.SubElement(box, S + "text", {
        "x": "16", "y": "22", "font-size": str(OUTPUT_FONT_SIZE), "fill": "CanvasText",
        "id": "output-prefix", "font-family": "VGA Terminal",
    }).text = "Output >"
    for row in range(OUTPUT_ROWS):
        line = ET.SubElement(box, S + "g", {"id": f"bf-output-line-{row + 1}"})
        # Empty rows reserve text positions only; no separators or filler text.
        events = []
        for time, value in output:
            lines = visible_lines(value)
            events.append((time, lines[row] if row < len(lines) else ""))
        states(line, events, duration, {
            "x": "80", "y": str(22 + row * OUTPUT_LINE_HEIGHT),
            "font-size": str(OUTPUT_FONT_SIZE), "fill": "CanvasText", "font-family": "VGA Terminal",
        })
    definitions = ET.SubElement(box, S + "defs")
    glow = ET.SubElement(definitions, S + "filter", {
        "id": "output-cursor-glow", "x": "-400%", "y": "-50%",
        "width": "900%", "height": "200%", "color-interpolation-filters": "sRGB",
    })
    ET.SubElement(glow, S + "feGaussianBlur", {"stdDeviation": "1.5", "result": "halo"})
    merge = ET.SubElement(glow, S + "feMerge")
    ET.SubElement(merge, S + "feMergeNode", {"in": "halo"})
    ET.SubElement(merge, S + "feMergeNode", {"in": "SourceGraphic"})
    cursor = ET.SubElement(box, S + "rect", {
        "id": "output-cursor", "x": "81", "y": "9", "width": "1.5", "height": "15",
        "fill": "#FFB000", "filter": "url(#output-cursor-glow)",
    })
    advance = terminal_cell_width(OUTPUT_FONT_SIZE)
    positions = [(time, 81 + len(visible_lines(value)[-1]) * advance) for time, value in output]
    track(cursor, "x", positions + [(duration, positions[-1][1])], duration)
    track(cursor, "y", [(time, 9 + (len(visible_lines(value))-1)*OUTPUT_LINE_HEIGHT) for time, value in output] + [(duration, 9 + (len(visible_lines(output[-1][1]))-1)*OUTPUT_LINE_HEIGHT)], duration)
    ET.SubElement(cursor, S + "animate", {
        "attributeName": "opacity", "values": "1;0;1", "keyTimes": "0;0.5;1",
        "calcMode": "discrete", "dur": "1s", "repeatCount": "indefinite",
        "begin": "landingEnter.begin", "restart": "never",
    })
    for index, events in enumerate(changes):
        cell = tape.find(S + f"text[@id='bf-cell-{index}']")
        attributes = dict(cell.attrib)
        attributes.pop("id")
        attributes["y"] = "65"
        tape.remove(cell)
        group = ET.SubElement(tape, S + "g", {"id": f"bf-cell-{index}"})
        states(group, events, duration, attributes)
        ascii_group = ET.SubElement(group, S + "g", {"id": f"bf-ascii-{index}"})
        characters = [(time, chr(value) if 33 <= value <= 126 else "") for time, value in events]
        states(ascii_group, characters, duration, {
            "x": attributes["x"], "y": "86", "text-anchor": "middle",
            "font-size": "16", "fill": cursor.get("fill"),
        })
    pointer = tape.find(S + "path[@id='bf-pointer']")
    tape.remove(pointer)
    group = ET.SubElement(tape, S + "g", {"id": "bf-pointer-motion"})
    group.append(pointer)
    track(group, "transform", [(time, f"{index*1000/15:.6f} 0") for time, index in moves] +
          [(duration, f"{moves[-1][1]*1000/15:.6f} 0")], duration,
          tag="animateTransform", type="translate")
    metadata = ET.SubElement(landing, S + "metadata")
    metadata.text = ("Offline execution of random/message.bf, verified against source/main.md. "
                     "8-bit wrapping cells; real memory sampled for playback. "
                     "The 15-cell viewport follows the pointer. No interpreter runs in CGI.")
    labels = [node for node in tape.findall(S+'text')]
    for index, label in enumerate(labels):
        attributes = dict(label.attrib)
        tape.remove(label)
        states(tape, [(t, origin+index) for t, origin in preset_trace.origins], duration, attributes)


def relayout_output(box, font_size=15, line_height=18):
    """Use the entire box width below the prefix; retain the whole message."""
    changes, moves, output, duration = preset_trace()
    advance = terminal_cell_width(font_size)
    prefix_x = 16 + terminal_cell_width(font_size)*8.5
    def layout(value):
        lines = [(prefix_x, '')]
        message = output[-1][1]
        for index, character in enumerate(value):
            x, line = lines[-1]
            capacity = int((484-x)/advance)
            if character == '\n': lines.append((16, '')); continue
            if not character.isspace() and (index == 0 or message[index-1].isspace()):
                end = index
                while end < len(message) and not message[end].isspace(): end += 1
                if line and len(line)+end-index > capacity:
                    lines.append((16, '')); x, line = lines[-1]
                    capacity = int((484-x)/advance)
            if len(line) >= capacity:
                lines.append((16, '')); x, line = lines[-1]
            lines[-1] = (x, line+character)
        return lines
    layouts = [(t, layout(value)) for t, value in output]
    rows = len(layouts[-1][1])
    baseline = 28 if font_size == 20 else 22
    box_height = baseline + (rows-1)*line_height + 14
    box.set('viewBox', f'0 0 500 {box_height}')
    box.set('height', str(box_height*1.2))
    box.set('data-rows', str(rows))
    box.find(S+'rect').set('height',str(box_height-2))
    for child in list(box):
        if child.get('id','').startswith('bf-output-line-') or child.get('id') == 'output-prefix':
            box.remove(child)
    ET.SubElement(box,S+'text',{'id':'output-prefix','x':'16','y':str(baseline),'font-size':str(font_size),
        'fill':'CanvasText','font-family':'VGA Terminal'}).text = 'Output >'
    for row in range(rows):
        line_group = ET.SubElement(box,S+'g',{'id':f'bf-output-line-{row+1}'})
        x = prefix_x if row == 0 else 16
        events = [(t, lines[row][1] if row < len(lines) else '') for t, lines in layouts]
        states(line_group,events,duration,{'x':str(x),'y':str(baseline+row*line_height),
            'font-size':str(font_size),'fill':'CanvasText','font-family':'VGA Terminal'})
    cursor = box.find(S+"rect[@id='output-cursor']")
    for animation in list(cursor):
        if animation.get('attributeName') in ('x','y'): cursor.remove(animation)
    cursor.set('x',str(prefix_x+1));cursor.set('y',str(baseline-font_size+2))
    cursor.set('height',str(font_size));cursor.set('width',str(font_size/10))
    xs = [(t, lines[-1][0]+1+len(lines[-1][1])*advance) for t,lines in layouts]
    ys = [(t, baseline-font_size+2+(len(lines)-1)*line_height) for t,lines in layouts]
    track(cursor,'x',xs+[(duration,xs[-1][1])],duration)
    track(cursor,'y',ys+[(duration,ys[-1][1])],duration)
    for definitions in box.findall(S+'defs'):
        for clip in list(definitions):
            if clip.get('id','').startswith('output-line-clip-'):
                definitions.remove(clip)
    # One text node per line, revealed by a clip instead of overlapping prefixes.
    for row in range(rows):
        group = box.find(S+f"g[@id='bf-output-line-{row+1}']")
        for child in list(group): group.remove(child)
        x = prefix_x if row == 0 else 16
        final_line = layouts[-1][1][row][1]
        clip_id = f'output-line-clip-{row}'
        clip = ET.SubElement(box.find(S+'defs'), S+'clipPath', {'id':clip_id})
        rect = ET.SubElement(clip,S+'rect',{'x':str(x),'y':str(baseline+row*line_height-font_size),
            'width':'0','height':str(line_height)})
        widths = [(t,len(lines[row][1])*advance if row<len(lines) else 0) for t,lines in layouts]
        track(rect,'width',widths+[(duration,widths[-1][1])],duration)
        ET.SubElement(group,S+'text',{'x':str(x),'y':str(baseline+row*line_height),
            'font-size':str(font_size),'fill':'CanvasText','font-family':'VGA Terminal',
            'clip-path':f'url(#{clip_id})','xml:space':'preserve'}).text=final_line
    return box_height*1.2
