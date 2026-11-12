#include <ctype.h>
#include <math.h>
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
    xmlBufferPtr values, times;
    int duration;
} Track;

static Track new_track(const TerminalTrace *trace)
{
    return Track{new_buffer(), new_buffer(), duration(trace)};
}

static void keyframe(Track *track, int time, const char *value)
{
    char formatted[64];
    if (xmlBufferLength(track->times)) {
        buffer_add(track->values, ";");
        buffer_add(track->times, ";");
    }
    snprintf(formatted, sizeof formatted, "%.10f", (double)time / track->duration);
    buffer_add(track->times, formatted);
    buffer_add(track->values, value);
}

static void numeric_keyframe(Track *track, int time, double value)
{
    char formatted[64];
    decimal(formatted, value);
    keyframe(track, time, formatted);
}

static void end_track(xmlNodePtr node, const char *name, const char *type, Track *track)
{
    xmlNodePtr animation = element(node, type ? "animateTransform" : "animate");
    char time[64], seconds[72];
    decimal(time, track->duration / 100.0);
    snprintf(seconds, sizeof seconds, "%ss", time);
    prop(animation, "attributeName", name);
    prop(animation, "values", (const char *)xmlBufferContent(track->values));
    prop(animation, "keyTimes", (const char *)xmlBufferContent(track->times));
    prop(animation, "dur", seconds);
    prop(animation, "begin", "landingEnter.begin");
    prop(animation, "restart", "never");
    prop(animation, "fill", "freeze");
    prop(animation, "calcMode", "discrete");
    if (type)
        prop(animation, "type", type);
    xmlBufferFree(track->values);
    xmlBufferFree(track->times);
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

static void tape_states(xmlNodePtr parent, const TerminalTrace *trace, int cell, int kind)
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
        end_track(group, "visibility", NULL, &track);
    }
}

static void render_tape(xmlNodePtr tape, const TerminalTrace *trace)
{
    xmlNodePtr pointer = require_id(tape, "bf-pointer-motion");
    xmlUnlinkNode(pointer);
    for (int cell = 0; cell < CELLS; cell++) {
        char id[32];
        snprintf(id, sizeof id, "bf-cell-%d", cell);
        xmlNodePtr group = element(tape, "g");
        prop(group, "id", id);
        tape_states(group, trace, cell, 0);
        snprintf(id, sizeof id, "bf-ascii-%d", cell);
        xmlNodePtr ascii = element(group, "g");
        prop(ascii, "id", id);
        tape_states(ascii, trace, cell, 1);
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
    end_track(pointer, "transform", "translate", &track);
    for (int cell = 0; cell < CELLS; cell++)
        tape_states(tape, trace, cell, 2);
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

static double render_output(xmlNodePtr box, const TerminalTrace *trace, int mobile)
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
    numeric(child(box, "rect"), "height", height - 2);
    xmlNodePtr cursor = require_id(box, "output-cursor");
    numeric(cursor, "x", prefix + 1);
    numeric(cursor, "y", baseline - font_size + 2);
    numeric(cursor, "width", font_size / 10.0);
    numeric(cursor, "height", font_size);
    Track x = new_track(trace), y = new_track(trace);
    double last_x = 0, last_y = 0;
    for (size_t i = 0; i < positions.size(); i++) {
        Position p = positions[i];
        last_x = (p.row ? 16 : prefix) + 1 + p.column * advance;
        last_y = baseline - font_size + 2 + p.row * line_height;
        numeric_keyframe(&x, time_of_output(i), last_x);
        numeric_keyframe(&y, time_of_output(i), last_y);
    }
    numeric_keyframe(&x, x.duration, last_x);
    numeric_keyframe(&y, y.duration, last_y);
    end_track(cursor, "x", NULL, &x);
    end_track(cursor, "y", NULL, &y);

    xmlNodePtr metadata = NULL;
    for (xmlNodePtr node = box->children; node; node = node->next)
        if (named(node, "metadata"))
            metadata = node;
    if (metadata)
        xmlUnlinkNode(metadata);
    size_t start = 0;
    for (size_t row = 0; row < rows; row++) {
        double left = row ? 16 : prefix, top = baseline + row * line_height;
        char id[64];
        snprintf(id, sizeof id, "output-line-clip-%u", (unsigned)row);
        xmlNodePtr clip = element(child(box, "defs"), "clipPath");
        prop(clip, "id", id);
        xmlNodePtr rect = element(clip, "rect");
        numeric(rect, "x", left);
        numeric(rect, "y", top - font_size);
        prop(rect, "width", "0");
        numeric(rect, "height", line_height);
        Track width = new_track(trace);
        size_t columns = 0;
        for (size_t i = 0; i < positions.size(); i++) {
            if (positions[i].row == row)
                columns = positions[i].column;
            numeric_keyframe(&width, time_of_output(i), columns * advance);
        }
        numeric_keyframe(&width, width.duration, columns * advance);
        end_track(rect, "width", NULL, &width);
        snprintf(value, sizeof value, "url(#%s)", id);
        xmlNodePtr group = element(box, "g");
        snprintf(id, sizeof id, "bf-output-line-%u", (unsigned)row + 1);
        prop(group, "id", id);
        xmlNodePtr text = element(group, "text");
        numeric(text, "x", left);
        numeric(text, "y", top);
        numeric(text, "font-size", font_size);
        prop(text, "fill", "CanvasText");
        prop(text, "font-family", "VGA Terminal");
        prop(text, "clip-path", value);
        xmlNodeSetSpacePreserve(text, 1);
        while (start < trace->output.size() && positions[start + 1].row < row)
            start++;
        if (start < trace->output.size() && trace->output[start] == '\n' &&
            positions[start + 1].row == row)
            start++;
        const auto line = utf8(trace->output.data() + start, columns);
        if (columns)
            xmlAddChild(text, xmlNewText(BAD_CAST line.c_str()));
        start += columns;
    }
    if (metadata)
        xmlAddChild(box, metadata);
    return height * 1.2;
}

void terminal_render(xmlNodePtr root, const TerminalTrace *trace, int mobile)
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
    render_tape(tape, trace);
    double tape_y = render_output(box, trace, mobile) + 45;
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
