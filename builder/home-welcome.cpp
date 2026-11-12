#include <float.h>
#include <limits.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include "home-welcome.h"

static int timed(xmlNodePtr node)
{
    return named(node, "set") || named(node, "animate");
}

static xmlChar *required_prop(xmlNodePtr node, const char *name)
{
    xmlChar *value = xmlGetProp(node, BAD_CAST name);
    if (!value)
        fail("Missing greeting attribute", name);
    return value;
}

static double seconds(xmlNodePtr node, const char *name, int optional)
{
    xmlChar *value = xmlGetProp(node, BAD_CAST name);
    if (!value && optional)
        return 0;
    if (!value)
        fail("Missing greeting time", name);
    char *end;
    double time = strtod((const char *)value, &end);
    if (end == (char *)value || strcmp(end, "s") != 0 || !isfinite(time) || time < 0)
        fail("Invalid greeting time", (const char *)value);
    xmlFree(value);
    return time;
}

static void time_range(xmlNodePtr node, double *first, double *last)
{
    if (timed(node)) {
        double begin = seconds(node, "begin", 0);
        double end = begin + seconds(node, "dur", 1);
        if (begin < *first)
            *first = begin;
        if (end > *last)
            *last = end;
    }
    for (xmlNodePtr item = node->children; item; item = item->next)
        time_range(item, first, last);
}

Welcome welcome_load(const char *language, const char *path)
{
    char *source = read_text(path);
    const char *start = strstr(source, "<svg");
    const char *end = NULL, *closing = start;
    while (closing && (closing = strstr(closing, "</svg>"))) {
        closing += 6;
        end = closing;
    }
    if (!start || !end || end - start > INT_MAX)
        fail("Missing greeting SVG", path);
    xmlDocPtr document = xmlReadMemory(start, (int)(end - start), path, NULL, XML_PARSE_NONET);
    free(source);
    if (!document || !xmlDocGetRootElement(document))
        fail("Cannot parse greeting SVG", path);
    double first = DBL_MAX, last = 0;
    time_range(xmlDocGetRootElement(document), &first, &last);
    if (last <= first)
        fail("Missing greeting animation", path);
    return Welcome{language, document, first, last - first};
}

typedef struct {
    xmlNodePtr node;
    xmlChar *attribute;
    double time;
    size_t order;
} Event;

typedef struct {
    xmlBufferPtr times, values;
    xmlChar *pending;
    double pending_time, total;
} Timeline;

static void flush_frame(Timeline *track)
{
    if (!track->pending)
        return;
    if (xmlBufferLength(track->times)) {
        buffer_add(track->times, ";");
        buffer_add(track->values, ";");
    }
    char value[64];
    decimal(value, track->pending_time / track->total);
    buffer_add(track->times, value);
    buffer_add(track->values, (const char *)track->pending);
    xmlFree(track->pending);
    track->pending = NULL;
}

static void frame(Timeline *track, double time, xmlChar *value)
{
    if (track->pending && fabs(track->pending_time - time) < 1e-9)
        xmlFree(track->pending);
    else
        flush_frame(track);
    track->pending = value;
    track->pending_time = time;
}

static void make_track(xmlNodePtr node, const char *attribute, Event *events,
                       size_t count, double first, double offset, double total)
{
    std::sort(events, events + count, [](const Event &left, const Event &right) {
        return left.time == right.time ? left.order < right.order : left.time < right.time;
    });
    int discrete = named(events[0].node, "set");
    Timeline track = {new_buffer(), new_buffer(), NULL, 0, total};
    xmlChar *initial = discrete ? xmlGetProp(node, BAD_CAST attribute) :
                                  required_prop(events[0].node, "from");
    frame(&track, 0, initial ? initial : xmlStrdup(BAD_CAST "hidden"));
    for (size_t i = 0; i < count; i++) {
        xmlNodePtr event = events[i].node;
        double begin = events[i].time - first + offset;
        if (named(event, "set") != discrete)
            fail("Mixed greeting animation modes", attribute);
        if (!discrete) {
            xmlChar *mode = xmlGetProp(event, BAD_CAST "calcMode");
            if (mode && !xmlStrEqual(mode, BAD_CAST "linear"))
                fail("Unsupported greeting animation mode", (const char *)mode);
            xmlFree(mode);
            frame(&track, begin, required_prop(event, "from"));
            begin += seconds(event, "dur", 0);
        }
        frame(&track, begin, required_prop(event, "to"));
        xmlUnlinkNode(event);
        xmlFreeNode(event);
    }
    frame(&track, total, xmlStrdup(track.pending));
    flush_frame(&track);
    xmlNodePtr animation = element(node, "animate");
    char time[64], duration[72];
    decimal(time, total);
    snprintf(duration, sizeof duration, "%ss", time);
    prop(animation, "attributeName", attribute);
    prop(animation, "values", (const char *)xmlBufferContent(track.values));
    prop(animation, "keyTimes", (const char *)xmlBufferContent(track.times));
    prop(animation, "dur", duration);
    prop(animation, "begin", "0s");
    prop(animation, "calcMode", discrete ? "discrete" : "linear");
    prop(animation, "repeatCount", "1");
    prop(animation, "fill", "freeze");
    xmlBufferFree(track.values);
    xmlBufferFree(track.times);
}

static void retime(xmlNodePtr node, double first, double offset, double total)
{
    if (node->type != XML_ELEMENT_NODE)
        return;
    size_t count = 0;
    for (xmlNodePtr item = node->children; item; item = item->next) {
        if (timed(item))
            count++;
        else
            retime(item, first, offset, total);
    }
    if (!count)
        return;
    std::vector<Event> events(count), track(count);
    size_t index = 0;
    for (xmlNodePtr item = node->children; item; item = item->next) {
        if (timed(item)) {
            events[index] = Event{item, required_prop(item, "attributeName"),
                                  seconds(item, "begin", 0), index};
            index++;
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (!events[i].attribute)
            continue;
        xmlChar *name = events[i].attribute;
        size_t length = 0;
        for (size_t j = i; j < count; j++) {
            if (xmlStrEqual(name, events[j].attribute)) {
                track[length++] = events[j];
                events[j].attribute = NULL;
            }
        }
        make_track(node, (const char *)name, track.data(), length, first, offset, total);
        for (size_t j = 0; j < length; j++)
            xmlFree(track[j].attribute);
    }
}

typedef struct { double left, top, right, bottom; } Bounds;

static Bounds empty_bounds(void)
{
    return Bounds{DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX};
}

static void include_point(Bounds *bounds, double x, double y)
{
    if (x < bounds->left) bounds->left = x;
    if (y < bounds->top) bounds->top = y;
    if (x > bounds->right) bounds->right = x;
    if (y > bounds->bottom) bounds->bottom = y;
}

static void path_bounds(xmlNodePtr path, int english, Bounds *bounds)
{
    double advance = 0;
    if (english) {
        xmlChar *value = required_prop(path, "transform");
        const char *shift = strstr((const char *)value, "translate(");
        if (!shift)
            fail("Missing stroke translation", "English greeting");
        advance = strtod(shift + sizeof "translate(" - 1, NULL);
        xmlFree(value);
    }
    xmlChar *data = required_prop(path, "d");
    const char *cursor = (const char *)data;
    double x = 0;
    int have_x = 0;
    while (*cursor) {
        char *end;
        double value = strtod(cursor, &end);
        if (end == cursor) {
            cursor++;
            continue;
        }
        if (have_x)
            include_point(bounds, x + advance, english ? -value : value);
        else
            x = value;
        have_x = !have_x;
        cursor = end;
    }
    xmlFree(data);
    if (have_x)
        fail("Unpaired stroke coordinate", "mobile greeting");
}

static void translate(xmlNodePtr group, double x, double y)
{
    char left[64], top[64], value[150];
    decimal(left, x);
    decimal(top, y);
    snprintf(value, sizeof value, "translate(%s %s)", left, top);
    prop(group, "transform", value);
}

static void mobile_layout(xmlNodePtr svg, const char *language)
{
    xmlNodePtr viewport = child(svg, "svg"), ink = child(viewport, "g");
    prop(viewport, "x", "8%");
    prop(viewport, "y", "18%");
    prop(viewport, "width", "84%");
    prop(viewport, "height", "64%");

    const int chinese = strcmp(language, "zh") == 0;
    const int english = strcmp(language, "en") == 0;
    const char *kind = chinese ? "g" : "path";
    const int counts_zh[] = {2, 1, 1}, counts_en[] = {7, 18, 6}, counts_ru[] = {17, 5, 15};
    const int *counts = chinese ? counts_zh : english ? counts_en : counts_ru;
    int strokes = 0;
    for (xmlNodePtr item = ink->children; item; item = item->next)
        strokes += named(item, kind);
    if (strokes != counts[0] + counts[1] + counts[2])
        fail("Mobile greeting stroke count changed", language);
    const double spacing = english ? 1200 : 20, padding = english ? 160 : 2;
    const double shifts[][2] = {{-85, 0}, {-237.5, 115}, {-550, 230}};
    Bounds total = empty_bounds();
    xmlNodePtr stroke = ink->children;
    for (int row = 0; row < 3; row++) {
        xmlNodePtr line = element(ink, "g");
        Bounds bounds = empty_bounds();
        for (int index = 0; index < counts[row]; index++) {
            while (stroke && stroke != line && !named(stroke, kind))
                stroke = stroke->next;
            if (!stroke || stroke == line)
                fail("Mobile greeting stroke count changed", language);
            if (!chinese)
                path_bounds(stroke, english, &bounds);
            xmlNodePtr next = stroke->next;
            xmlUnlinkNode(stroke);
            xmlAddChild(line, stroke);
            stroke = next;
        }
        if (chinese) {
            translate(line, shifts[row][0], shifts[row][1]);
        } else {
            if (bounds.left == DBL_MAX)
                fail("Missing stroke coordinates", language);
            double x = -(bounds.left + bounds.right) / 2, y = row * spacing;
            translate(line, x, y);
            include_point(&total, bounds.left + x, bounds.top + y);
            include_point(&total, bounds.right + x, bounds.bottom + y);
        }
    }
    if (chinese) {
        prop(viewport, "viewBox", "-270 -15 540 350");
        prop(viewport, "data-lines", "你好，|我是|ForgetWolf");
    } else {
        char left[64], top[64], width[64], height[64], view_box[270];
        decimal(left, total.left - padding);
        decimal(top, total.top - padding);
        decimal(width, total.right - total.left + 2 * padding);
        decimal(height, total.bottom - total.top + 2 * padding);
        snprintf(view_box, sizeof view_box, "%s %s %s %s", left, top, width, height);
        prop(viewport, "viewBox", view_box);
        prop(viewport, "data-lines", english ? "Hello,|ForgetWolf|here." :
             "Здравствуйте,|здесь|ФоргетВолк.");
    }
}

xmlNodePtr welcome_render(const Welcome *welcome, xmlNodePtr group, double offset,
                          double total, int mobile)
{
    xmlNodePtr svg = xmlDocCopyNode(xmlDocGetRootElement(welcome->document), group->doc, 1);
    if (!svg || !xmlAddChild(group, svg))
        fail("Cannot copy greeting", welcome->language);
    retime(svg, welcome->first, offset, total);
    prop(svg, "height", "100%");
    prop(svg, "data-language", welcome->language);
    numeric(svg, "data-start", offset);
    numeric(svg, "data-end", offset + welcome->length);
    if (mobile)
        mobile_layout(svg, welcome->language);
    return svg;
}
