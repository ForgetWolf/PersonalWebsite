#include <ctype.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include "home-terminal.h"

enum { CELLS = 15, SAMPLE_INTERVAL = 64, STEP_LIMIT = 10000000 };

typedef struct {
    int origin, pointer;
    unsigned char cells[CELLS];
} Snapshot;

struct TerminalTrace {
    std::vector<unsigned char> output;
    std::vector<Snapshot> samples{Snapshot{}};
    xmlDocPtr glyphs = nullptr;
    xmlNodePtr glyph[127]{};
    int units = 0;
    int last_cell = 0;
};

static Snapshot snapshot(const unsigned char *memory, int pointer)
{
    int origin = pointer / CELLS * CELLS;
    Snapshot state{origin, pointer - origin, {}};
    memcpy(state.cells, memory + origin, CELLS);
    return state;
}

static void output_byte(TerminalTrace *trace, unsigned char value, const Snapshot *pending,
                        size_t count)
{
    for (const Snapshot &sample : {pending[0], pending[count / 2], pending[count - 1]}) {
        trace->samples.push_back(sample);
        if (sample.origin + CELLS - 1 > trace->last_cell)
            trace->last_cell = sample.origin + CELLS - 1;
    }
    trace->output.push_back(value);
}

static std::string utf8(const unsigned char *bytes, size_t length)
{
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; i++) {
        unsigned char value = bytes[i];
        if (value < 32 && value != '\t' && value != '\n' && value != '\r')
            fail("Unsupported control byte in", "Brainfuck output");
        if (value >= 128) {
            result.push_back(static_cast<char>(0xc0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 63)));
        } else {
            result.push_back(static_cast<char>(value));
        }
    }
    return result;
}

static int whitespace(unsigned char value)
{
    return isspace(value) || (value >= 28 && value <= 31) || value == 0x85 || value == 0xa0;
}

static void verify_message(const TerminalTrace *trace)
{
    char *source = read_text("source/main.md");
    char *line = source + (strncmp(source, "\xef\xbb\xbf", 3) == 0 ? 3 : 0);
    unsigned delimiters = 0;
    while (*line && delimiters < 2) {
        size_t length = strcspn(line, "\n");
        if ((length == 3 || (length == 4 && line[3] == '\r')) &&
            strncmp(line, "---", 3) == 0)
            delimiters++;
        line += length + (line[length] == '\n');
    }
    if (delimiters != 2)
        fail("Missing front matter", "source/main.md");
    char *end = line;
    for (char *cursor = line; *cursor; cursor++)
        if (*cursor != '\r' || cursor[1] != '\n')
            *end++ = *cursor;
    while (end > line && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    while (isspace((unsigned char)*line))
        line++;
    size_t start = 0, length = trace->output.size();
    while (length > start && whitespace(trace->output[length - 1]))
        length--;
    while (start < length && whitespace(trace->output[start]))
        start++;
    if (utf8(trace->output.data() + start, length - start) != line)
        fail("Brainfuck output differs from", "source/main.md");
    free(source);
}

static int int_prop(xmlNodePtr node, const char *key)
{
    xmlChar *value = xmlGetProp(node, BAD_CAST key);
    if (!value)
        fail("Missing glyph attribute", key);
    int result = atoi((const char *)value);
    xmlFree(value);
    return result;
}

TerminalTrace *terminal_execute(void)
{
    char *code = read_text("random/message.bf");
    size_t length = 0;
    for (const char *input = code; *input; input++)
        if (strchr("+-<>[].,", *input))
            code[length++] = *input;
    std::vector<size_t> jumps(length), brackets;
    for (size_t i = 0; i < length; i++) {
        if (code[i] == '[')
            brackets.push_back(i);
        else if (code[i] == ']') {
            if (brackets.empty())
                fail("Unmatched bracket", "random/message.bf");
            size_t opening = brackets.back();
            brackets.pop_back();
            jumps[opening] = i;
            jumps[i] = opening;
        }
    }
    if (!brackets.empty())
        fail("Unmatched bracket", "random/message.bf");

    auto *trace = new TerminalTrace;
    std::vector<unsigned char> memory(STEP_LIMIT + CELLS);
    std::vector<Snapshot> pending;
    size_t step = 0;
    int pointer = 0;
    for (size_t pc = 0; pc < length; pc++, step++) {
        if (step >= STEP_LIMIT)
            fail("Instruction budget exceeded", "random/message.bf");
        char operation = code[pc];
        unsigned char value = memory[pointer];
        switch (operation) {
        case '+': memory[pointer]++; break;
        case '-': memory[pointer]--; break;
        case '>': pointer++; break;
        case '<':
            if (--pointer < 0)
                fail("Negative memory address", "random/message.bf");
            break;
        case '[': if (!value) pc = jumps[pc]; break;
        case ']': if (value) pc = jumps[pc]; break;
        case ',': fail("Interactive input is unsupported", "random/message.bf"); break;
        }
        if (step % SAMPLE_INTERVAL == 0)
            pending.push_back(snapshot(memory.data(), pointer));
        if (operation == '.') {
            pending.push_back(snapshot(memory.data(), pointer));
            output_byte(trace, value, pending.data(), pending.size());
            pending.clear();
        }
    }
    free(code);
    if (trace->output.empty())
        fail("Expected output from", "random/message.bf");
    verify_message(trace);

    trace->glyphs = read_xml("builder/templates/din-glyphs.xml");
    xmlNodePtr root = xmlDocGetRootElement(trace->glyphs);
    trace->units = int_prop(root, "units");
    for (xmlNodePtr item = root->children; item; item = item->next) {
        if (named(item, "glyph")) {
            int codepoint = int_prop(item, "code");
            if (codepoint >= 0 && codepoint < (int)(sizeof trace->glyph / sizeof trace->glyph[0]))
                trace->glyph[codepoint] = item;
        }
    }
    printf("Brainfuck verified: %u characters, %u instructions\n",
           (unsigned)trace->output.size(), (unsigned)step);
    return trace;
}

static int duration(const TerminalTrace *trace)
{
    return 50 + (int)trace->output.size() * 9;
}

static int time_of_sample(size_t sample)
{
    return sample ? 25 + (int)sample * 3 : 0;
}

typedef struct {
    std::vector<std::pair<double, std::string>> frames;
    int duration;
} Track;

static Track new_track(const TerminalTrace *trace)
{
    return Track{{}, duration(trace)};
}

static void keyframe(Track *track, double time, const char *value)
{
    if (!track->frames.empty() && track->frames.back().first == time)
        track->frames.back().second = value;
    else
        track->frames.emplace_back(time, value);
}

static void numeric_keyframe(Track *track, double time, double value)
{
    char formatted[64];
    decimal(formatted, value);
    keyframe(track, time, formatted);
}

static std::string css_value(const char *name, const std::string &value)
{
    if (strcmp(name, "transform") == 0) {
        const auto space = value.find(' ');
        return "translate(" + value.substr(0, space) + "px," + value.substr(space + 1) + "px)";
    }
    return strcmp(name, "visibility") == 0 ? value : value + "px";
}

static void end_track(TerminalCss &css, xmlNodePtr node, const char *name,
                      const Track &track, bool linear = false)
{
    std::string frames;
    for (const auto &[time, value] : track.frames) {
        char percent[64];
        decimal(percent, time * 100 / track.duration);
        frames += std::string(percent) + "%{" + name + ':' + css_value(name, value) + '}';
    }
    const std::string base = std::string(name) + ':' + css_value(name, track.frames.back().second) + ';';
    css.animate(node, base, css.keyframes(frames) + ' ' + std::to_string(track.duration * 10) +
                "ms " + (linear ? "linear" : "steps(1,end)") + " backwards");
}

static void outline(xmlNodePtr parent, const TerminalTrace *trace, const char *value,
                    double x, int y, int size, const char *fill, int muted)
{
    int width = 0;
    const unsigned char *character = (const unsigned char *)value;
    for (; *character; character++) {
        xmlNodePtr glyph = trace->glyph[*character];
        if (!glyph)
            fail("Missing DIN glyph", value);
        width += int_prop(glyph, "advance");
    }
    double scale = (double)size / trace->units;
    char transform[128];
    /* Match the Python outline formatter's round-to-even at six decimals. */
    double left = (double)(nearbyintl((x - width * scale / 2) * 1000000.0L) / 1000000.0L);
    snprintf(transform, sizeof transform, "translate(%.6f %.6f) scale(%.9f %.9f)",
             left, (double)y, scale, -scale);
    xmlNodePtr group = element(parent, "g");
    prop(group, "transform", transform);
    prop(group, "fill", fill);
    prop(group, "aria-label", value);
    prop(group, "data-text", value);
    prop(group, "data-font", "Alte DIN 1451 Mittelschrift");
    if (muted)
        prop(group, "opacity", ".55");
    int position = 0;
    for (character = (const unsigned char *)value; *character; character++) {
        xmlNodePtr glyph = trace->glyph[*character];
        xmlChar *path = xmlGetProp(glyph, BAD_CAST "path");
        xmlNodePtr shape = element(group, "path");
        prop(shape, "d", (const char *)path);
        xmlFree(path);
        snprintf(transform, sizeof transform, "translate(%d 0)", position);
        prop(shape, "transform", transform);
        position += int_prop(glyph, "advance");
    }
}

static int tape_value(const Snapshot *state, int cell, int kind)
{
    if (kind == 2)
        return state->origin + cell;
    int value = state->cells[cell];
    return kind == 0 || (value >= 33 && value <= 126) ? value : -1;
}

static void tape_states(xmlNodePtr parent, const TerminalTrace *trace, int cell, int kind,
                       TerminalCss &css)
{
    const size_t count = trace->samples.size();
    size_t values = kind == 2 ? (size_t)trace->last_cell + 1 : 256;
    std::vector<unsigned char> seen(values);
    char coordinate[64];
    decimal(coordinate, 1000 * (cell + .5) / CELLS);
    double x = strtod(coordinate, NULL);
    for (size_t sample = 0; sample < count; sample++) {
        int value = tape_value(&trace->samples[sample], cell, kind);
        if (value < 0 || seen[value])
            continue;
        seen[value] = 1;
        char text[32];
        if (kind == 1)
            snprintf(text, sizeof text, "%c", value);
        else
            snprintf(text, sizeof text, "%d", value);
        xmlNodePtr group = element(parent, "g");
        int visible = tape_value(&trace->samples[0], cell, kind) == value;
        prop(group, "visibility", visible ? "visible" : "hidden");
        prop(group, "data-state", text);
        outline(group, trace, text, x, kind == 0 ? 65 : kind == 1 ? 86 : 16,
                kind == 0 ? 24 : kind == 1 ? 16 : 14,
                kind == 1 ? "#FFB000" : "CanvasText", kind == 2);
        Track track = new_track(trace);
        keyframe(&track, 0, visible ? "visible" : "hidden");
        for (size_t i = 1; i < count; i++) {
            int next = tape_value(&trace->samples[i], cell, kind) == value;
            if (next != visible) {
                visible = next;
                keyframe(&track, time_of_sample(i), visible ? "visible" : "hidden");
            }
        }
        keyframe(&track, track.duration, visible ? "visible" : "hidden");
        end_track(css, group, "visibility", track);
    }
}

static void render_tape(xmlNodePtr tape, const TerminalTrace *trace, TerminalCss &css)
{
    xmlNodePtr pointer = require_id(tape, "bf-pointer-motion");
    xmlUnlinkNode(pointer);
    for (int cell = 0; cell < CELLS; cell++) {
        char id[32];
        snprintf(id, sizeof id, "bf-cell-%d", cell);
        xmlNodePtr group = element(tape, "g");
        prop(group, "id", id);
        tape_states(group, trace, cell, 0, css);
        snprintf(id, sizeof id, "bf-ascii-%d", cell);
        xmlNodePtr ascii = element(group, "g");
        prop(ascii, "id", id);
        tape_states(ascii, trace, cell, 1, css);
    }
    xmlAddChild(tape, pointer);
    Track track = new_track(trace);
    int previous = -1;
    char position[64];
    for (size_t i = 0; i < trace->samples.size(); i++) {
        int current = trace->samples[i].pointer;
        if (current != previous) {
            previous = current;
            snprintf(position, sizeof position, "%.6f 0", current * 1000.0 / CELLS);
            keyframe(&track, time_of_sample(i), position);
        }
    }
    keyframe(&track, track.duration, position);
    end_track(css, pointer, "transform", track);
    for (int cell = 0; cell < CELLS; cell++)
        tape_states(tape, trace, cell, 2, css);
}

typedef struct { size_t row, column; } Position;

static std::vector<Position> layout_message(const TerminalTrace *trace, double prefix, double advance)
{
    std::vector<Position> positions(trace->output.size() + 1);
    Position position{};
    positions[0] = position;
    for (size_t i = 0; i < trace->output.size(); i++) {
        unsigned char letter = trace->output[i];
        if (letter == '\n') {
            position = Position{position.row + 1, 0};
        } else {
            size_t capacity = (size_t)((484 - (position.row ? 16 : prefix)) / advance);
            size_t word = 0;
            if (!whitespace(letter) && (!i || whitespace(trace->output[i - 1]))) {
                while (i + word < trace->output.size() && !whitespace(trace->output[i + word]))
                    word++;
            }
            if (position.column >= capacity ||
                (position.column && position.column + word > capacity))
                position = Position{position.row + 1, 0};
            position.column++;
        }
        positions[i + 1] = position;
    }
    return positions;
}

static int time_of_output(size_t index)
{
    return index ? 25 + (int)index * 9 : 0;
}

// Timeline times are in hundredths of a second.
static constexpr double pop_duration = 4.8, rise_duration = pop_duration * .75;
static constexpr double paper_feed_duration = 16;

static void translate(xmlNodePtr node, double x, double y)
{
    char a[64], b[64], value[152];
    decimal(a, x);
    decimal(b, y);
    snprintf(value, sizeof value, "translate(%s %s)", a, b);
    prop(node, "transform", value);
}

static std::string delay_at(double time)
{
    char offset[64];
    decimal(offset, time * 10);
    return std::string(offset) + "ms";
}

static void show_at(TerminalCss &css, xmlNodePtr node, double time)
{
    css.animate(node, "", "terminal-show 1ms steps(1,end) " + delay_at(time - .1) + " backwards");
}

static void add_rising_glyph(xmlNodePtr row, const std::string &reference,
                             double left, double baseline, int font_size, int units,
                             double time, TerminalCss &css)
{
    xmlNodePtr slot = element(row, "g");
    translate(slot, left, baseline);
    prop(slot, "clip-path", "url(#output-glyph-slot)");
    css.animate(slot, "visibility:hidden;", "terminal-rising-visible 36ms steps(1,end) " + delay_at(time));

    xmlNodePtr moving = element(slot, "g");
    xmlNodePtr glyph = element(moving, "use");
    prop(glyph, "href", reference.c_str());
    char number[64];
    decimal(number, (double)font_size / units);
    char value[160];
    snprintf(value, sizeof value, "scale(%s -%s)", number, number);
    prop(glyph, "transform", value);

    css.animate(moving, "", std::string(font_size == 20 ? "terminal-rise-narrow" : "terminal-rise-wide") +
                " 36ms steps(1,end) " + delay_at(time));
}

static double render_output(xmlNodePtr box, const TerminalTrace *trace, bool mobile, TerminalCss &css)
{
    int font_size = mobile ? 20 : 15, line_height = mobile ? 24 : 18;
    int baseline = mobile ? 28 : 22;
    double advance = font_size * .5, prefix = 16 + advance * 8.5;
    const auto positions = layout_message(trace, prefix, advance);
    size_t rows = positions.back().row + 1;
    double height = baseline + (rows - 1) * line_height + 14;
    char value[96];
    snprintf(value, sizeof value, "0 0 500 %.0f", height);
    prop(box, "viewBox", value);
    numeric(box, "height", height * 1.2);
    numeric(box, "data-rows", (double)rows);
    numeric(box, "data-line-height", line_height);
    prop(box, "overflow", "hidden");
    const double travel = (rows - 1) * line_height;
    xmlNodePtr background = child(box, "rect");
    numeric(background, "y", 1 + travel);
    numeric(background, "height", height - 2 - travel);

    xmlNodePtr defs = child(box, "defs");
    xmlNodePtr paper_clip = element(defs, "clipPath");
    prop(paper_clip, "id", "output-paper-clip");
    xmlNodePtr paper_edge = xmlCopyNode(background, 1);
    xmlAddChild(paper_clip, paper_edge);
    xmlNodePtr viewport = element(box, "g");
    prop(viewport, "clip-path", "url(#output-paper-clip)");
    xmlNodePtr paper = element(viewport, "g");
    prop(paper, "id", "output-paper");
    translate(paper, 0, travel);

    xmlNodePtr cursor = require_id(box, "output-cursor");
    xmlNodePtr label = require_id(box, "output-prefix");
    xmlUnlinkNode(cursor);
    xmlUnlinkNode(label);
    xmlAddChild(paper, label);
    numeric(cursor, "x", prefix + 1);
    numeric(cursor, "y", baseline);
    numeric(cursor, "width", floor(advance));
    numeric(cursor, "height", 2);
    xmlNodePtr glow = require_id(defs, "output-cursor-glow");
    prop(glow, "x", "-50%");
    prop(glow, "width", "200%");
    prop(glow, "y", "-250%");
    prop(glow, "height", "600%");

    xmlNodePtr slot = element(defs, "clipPath");
    prop(slot, "id", "output-glyph-slot");
    xmlNodePtr slot_edge = element(slot, "rect");
    numeric(slot_edge, "y", -font_size);
    numeric(slot_edge, "width", advance);
    numeric(slot_edge, "height", font_size);

    xmlDocPtr vga = read_xml("builder/templates/vga-glyphs.xml");
    xmlNodePtr vga_root = xmlDocGetRootElement(vga);
    const int units = int_prop(vga_root, "units");
    std::string glyph_references[256];
    for (xmlNodePtr node = vga_root->children; node; node = node->next) {
        if (!named(node, "glyph"))
            continue;
        int code = int_prop(node, "code");
        if (code < 0 || code > 255)
            continue;
        std::string id = "output-glyph-" + std::to_string(code);
        glyph_references[code] = "#" + id;
        xmlNodePtr glyph = element(defs, "path");
        prop(glyph, "id", id.c_str());
        xmlChar *path = xmlGetProp(node, BAD_CAST "path");
        if (!path)
            fail("Missing VGA outline", id.c_str());
        prop(glyph, "d", reinterpret_cast<const char *>(path));
        xmlFree(path);
    }
    xmlFreeDoc(vga);

    // Finish feeding the paper before placing any ink on the new row.
    std::vector<double> typing_times(positions.size());
    std::vector<double> feed_starts(rows), row_ready(rows);
    for (size_t i = 1; i < positions.size(); ++i) {
        const size_t row = positions[i].row;
        double time = std::max<double>(time_of_output(i), typing_times[i - 1] + pop_duration);
        if (row != positions[i - 1].row) {
            feed_starts[row] = time;
            row_ready[row] = time + paper_feed_duration;
        }
        typing_times[i] = std::max(time, row_ready[row]);
    }
    const int output_duration = std::max(duration(trace),
        static_cast<int>(ceil(typing_times.back() + pop_duration)) + 1);
    Track x = new_track(trace), y = new_track(trace), feed = new_track(trace);
    Track caret_visibility = new_track(trace);
    Track paper_top = new_track(trace), paper_height = new_track(trace);
    x.duration = y.duration = feed.duration = caret_visibility.duration =
        paper_top.duration = paper_height.duration = output_duration;
    auto feed_frame = [&](double time, double offset) {
        char number[64], value[72];
        decimal(number, offset);
        snprintf(value, sizeof value, "0 %s", number);
        keyframe(&feed, time, value);
        numeric_keyframe(&paper_top, time, 1 + offset);
        numeric_keyframe(&paper_height, time, height - 2 - offset);
    };
    feed_frame(0, travel);
    keyframe(&caret_visibility, 0, "visible");
    double last_x = prefix + 1, last_y = baseline;
    std::vector<std::vector<size_t>> cells(rows);
    for (size_t i = 0; i < positions.size(); ++i) {
        const Position p = positions[i];
        const double time = typing_times[i];
        const bool wrap = i && p.row != positions[i - 1].row;
        const unsigned char byte = i ? trace->output[i - 1] : ' ';
        const bool pop = !whitespace(byte) && !glyph_references[byte].empty();
        last_x = (p.row ? 16 : prefix) + 1 + p.column * advance;
        last_y = baseline + p.row * line_height;
        if (wrap) {
            const double offset = travel - p.row * line_height;
            feed_frame(feed_starts[p.row], offset + line_height);
            feed_frame(row_ready[p.row], offset);
            keyframe(&caret_visibility, feed_starts[p.row], "hidden");
            keyframe(&caret_visibility, row_ready[p.row], "visible");
            if (pop)
                numeric_keyframe(&x, time, last_x - advance);
        }
        numeric_keyframe(&x, time + (pop ? pop_duration : 0), last_x);
        numeric_keyframe(&y, time, last_y);
        if (p.column)
            cells[p.row].push_back(i);
    }
    numeric_keyframe(&x, x.duration, last_x);
    numeric_keyframe(&y, y.duration, last_y);
    keyframe(&caret_visibility, caret_visibility.duration, "visible");
    feed_frame(feed.duration, 0);
    end_track(css, cursor, "x", x);
    end_track(css, cursor, "y", y);
    end_track(css, cursor, "visibility", caret_visibility);
    css.animate(cursor, "", "terminal-blink 1s steps(1,end) infinite");
    end_track(css, paper, "transform", feed, true);
    for (const auto edge : {background, paper_edge}) {
        end_track(css, edge, "y", paper_top, true);
        end_track(css, edge, "height", paper_height, true);
    }

    for (size_t row = 0; row < rows; ++row) {
        const double left = row ? 16 : prefix, top = baseline + row * line_height;
        if (cells[row].empty())
            continue;
        xmlNodePtr group = element(paper, "g");
        prop(group, "id", ("bf-output-line-" + std::to_string(row + 1)).c_str());
        if (row)
            show_at(css, group, row_ready[row]);
        xmlNodePtr rising = element(group, "g");
        prop(rising, "fill", "CanvasText");
        prop(rising, "aria-hidden", "true");
        prop(rising, "pointer-events", "none");
        xmlNodePtr text = element(group, "text");
        numeric(text, "x", left);
        numeric(text, "y", top);
        numeric(text, "font-size", font_size);
        prop(text, "fill", "CanvasText");
        prop(text, "font-family", "VGA Terminal");
        xmlNodeSetSpacePreserve(text, 1);
        size_t column = 0;
        for (const size_t i : cells[row]) {
            const unsigned char byte = trace->output[i - 1];
            const double time = typing_times[i];
            const auto &reference = glyph_references[byte];
            const bool pop = !whitespace(byte) && !reference.empty();
            const double x = left + column++ * advance;
            if (pop)
                add_rising_glyph(rising, reference, x, top, font_size, units, time, css);
            const auto character = utf8(&byte, 1);
            // A letter enters the rendered and selectable text together.
            xmlNodePtr span = element(text, "tspan");
            numeric(span, "x", x);
            xmlAddChild(span, xmlNewText(BAD_CAST character.c_str()));
            show_at(css, span, time + (pop ? rise_duration : 0));
        }
        const double end = typing_times[cells[row].back()] + pop_duration;
        css.animate(rising, "opacity:0;", "terminal-lift-layer 1ms steps(1,end) " + delay_at(end - .1) +
                    " backwards");
    }
    xmlAddChild(paper, cursor);
    return height * 1.2;
}

void terminal_render(xmlNodePtr root, const TerminalTrace *trace, bool mobile, TerminalCss &css)
{
    xmlNodePtr landing = require_id(root, "landing");
    xmlNodePtr stage = require_id(landing, "terminal-stage");
    xmlNodePtr tape = require_id(stage, "brainfuck-tape");
    xmlNodePtr box = require_id(stage, "brainfuck-output");
    const auto message = utf8(trace->output.data(), trace->output.size());
    prop(landing, "data-output", message.c_str());
    numeric(landing, "data-output-duration", duration(trace) / 100.0);
    xmlBufferPtr label = new_buffer();
    buffer_add(label, "Brainfuck output: ");
    buffer_add(label, message.c_str());
    prop(box, "aria-label", (const char *)xmlBufferContent(label));
    xmlBufferFree(label);
    render_tape(tape, trace, css);
    double tape_y = render_output(box, trace, mobile, css) + 45;
    numeric(tape, "y", tape_y);
    double height = tape_y + 75.6;
    numeric(stage, "height", height);
    char value[64], attribute[96];
    decimal(value, height);
    snprintf(attribute, sizeof attribute, "0 0 600 %s", value);
    prop(stage, "viewBox", attribute);
    decimal(value, -height / 2);
    snprintf(attribute, sizeof attribute, "translate(0 %s)", value);
    prop(stage->parent, "transform", attribute);
}

void terminal_free(TerminalTrace *trace)
{
    xmlFreeDoc(trace->glyphs);
    delete trace;
}
