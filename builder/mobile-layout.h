#pragma once

#include <cfloat>
#include "home-svg.h"

// Three-line layout from the original welcome builder.

static xmlChar *required_prop(xmlNodePtr node, const char *name)
{
    xmlChar *value = xmlGetProp(node, BAD_CAST name);
    if (!value)
        fail("Missing greeting attribute", name);
    return value;
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
