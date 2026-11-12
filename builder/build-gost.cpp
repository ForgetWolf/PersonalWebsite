#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

enum {
    CYRILLIC = 0x0400, CYRILLIC_LENGTH = 256, RUSSIAN_LETTERS = 66,
    SCALE = 76, RADIUS = 21, BEARING = 28, CONTOUR_POINTS = 18
};

typedef struct {
    double x, y;
} Point;

typedef struct {
    std::string path;
    double width;
} Glyph;

static void fail(const char *message, const char *detail)
{
    fprintf(stderr, "%s: %s\n", message, detail);
    exit(EXIT_FAILURE);
}

static char *skip_space(char *text)
{
    while (isspace((unsigned char)*text))
        text++;
    return text;
}

static int is_russian(unsigned code)
{
    return code == 0x0401 || (code >= 0x0410 && code <= 0x044f) || code == 0x0451;
}

static unsigned glyph_code(const char *line)
{
    const unsigned char *key = (const unsigned char *)line;
    if (key[0] != '"' || key[1] < 0xd0 || key[1] > 0xd3 ||
        (key[2] & 0xc0) != 0x80 || key[3] != '"' || key[4] != ':')
        return 0;
    return ((unsigned)(key[1] & 31) << 6) | (key[2] & 63);
}

static void read_glyphs(FILE *input, Glyph glyphs[CYRILLIC_LENGTH])
{
    char line[16384];
    while (fgets(line, sizeof line, input)) {
        if (!strchr(line, '\n') && !feof(input))
            fail("Glyph input line is too long", "GOST-font.coffee");
        unsigned code = glyph_code(skip_space(line));
        if (!is_russian(code))
            continue;
        char name[24];
        snprintf(name, sizeof name, "U+%04X", code);
        Glyph *glyph = &glyphs[code - CYRILLIC];
        if (!glyph->path.empty() || !fgets(line, sizeof line, input))
            fail("Invalid glyph path record", name);
        char *path = skip_space(line);
        if (strncmp(path, "d: \"", 4) != 0)
            fail("Missing glyph path", name);
        path += 4;
        char *end = strchr(path, '"');
        if (!end)
            fail("Unterminated glyph path", name);
        glyph->path.assign(path, end);

        if (!fgets(line, sizeof line, input))
            fail("Missing glyph width", name);
        char *width = skip_space(line);
        if (strncmp(width, "w:", 2) != 0)
            fail("Missing glyph width", name);
        width = skip_space(width + 2);
        glyph->width = strtod(width, &end);
        if (width == end || *skip_space(end) || !isfinite(glyph->width) ||
            glyph->width <= 0 || glyph->width > 100)
            fail("Invalid glyph width", name);
    }
    if (ferror(input))
        fail("Cannot read", "GOST-font.coffee");

    /* The original renderer supplied a capital Be where the source omitted it. */
    static const char capital_be[] =
        "m 0,0 l 0,-9.375 l 3.75,0 m -3.75,4.375 l 2.5,0 l .946,.256 "
        "l .673,.673 l .256,.946 l 0,1.25 l -.256,.946 l -.673,.673 "
        "l -.946,.256 l -2.5,0";
    Glyph *be = &glyphs[0x0411 - CYRILLIC];
    if (be->path.empty())
        *be = Glyph{capital_be, 4.375};
}

static void add_segment(Point outline[CONTOUR_POINTS], Point start, Point end)
{
    const double pi = acos(-1.0);
    double angle = atan2(end.y - start.y, end.x - start.x);
    const Point centers[] = {end, start};
    for (unsigned cap = 0; cap < 2; cap++) {
        double rotation = angle + (cap == 0 ? pi / 2 : -pi / 2);
        for (unsigned step = 0; step < 9; step++) {
            double theta = rotation - pi * step / 8;
            outline[cap * 9 + step] = Point{
                nearbyint(centers[cap].x + RADIUS * cos(theta)),
                nearbyint(centers[cap].y + RADIUS * sin(theta))
            };
        }
    }
}

static std::vector<Point> make_outline(char *cursor, const char *name)
{
    /* Each segment has an explicit SVG line command in this input format. */
    std::vector<Point> outline;
    Point previous = {0, 0};
    int moved = 0;
    while (*(cursor = skip_space(cursor))) {
        char command = *cursor++;
        if ((command != 'm' && command != 'l') || (command == 'l' && !moved))
            fail("Unsupported SVG command in", name);
        cursor = skip_space(cursor);
        char *end;
        double dx = strtod(cursor, &end);
        if (cursor == end || *(end = skip_space(end)) != ',')
            fail("Invalid x coordinate in", name);
        cursor = skip_space(end + 1);
        double dy = strtod(cursor, &end);
        if (cursor == end || !isfinite(dx) || !isfinite(dy))
            fail("Invalid y coordinate in", name);
        Point next = {previous.x + dx, previous.y + dy};
        if (fabs(next.x * SCALE) > 30000 || fabs(next.y * SCALE) > 30000)
            fail("Outline exceeds TrueType coordinate range", name);
        Point start = {previous.x * SCALE, -previous.y * SCALE};
        Point finish = {next.x * SCALE, -next.y * SCALE};
        if (command == 'l' && (start.x != finish.x || start.y != finish.y)) {
            size_t next_contour = outline.size();
            outline.resize(next_contour + CONTOUR_POINTS);
            add_segment(outline.data() + next_contour, start, finish);
        }
        moved = 1;
        previous = next;
        cursor = end;
    }
    if (outline.empty())
        fail("Empty glyph outline", name);
    return outline;
}

static void write_glyph(FILE *output, Glyph &glyph, unsigned code, unsigned index)
{
    char name[24];
    snprintf(name, sizeof name, "uni%04X", code);
    if (glyph.path.empty())
        fail("Missing Russian glyph", name);
    const auto outline = make_outline(glyph.path.data(), name);
    int left = INT_MAX, right = INT_MIN;
    for (const Point &point : outline) {
        int x = (int)point.x;
        if (x < left)
            left = x;
        if (x > right)
            right = x;
    }
    int advance = (int)nearbyint(glyph.width * SCALE);
    if (advance < right - left)
        advance = right - left;
    fprintf(output, "StartChar: %s\nEncoding: %u %u %u\nWidth: %d\n"
                    "Flags: W\nLayerCount: 2\nFore\nSplineSet\n",
            name, code, code, index, advance + 2 * BEARING);
    for (size_t i = 0; i < outline.size(); i += CONTOUR_POINTS) {
        const Point *points = outline.data() + i;
        fprintf(output, "%d %d m 1\n", (int)points[0].x + BEARING - left, (int)points[0].y);
        unsigned last = CONTOUR_POINTS;
        if (points[last - 1].x == points[0].x && points[last - 1].y == points[0].y)
            last--;
        for (unsigned j = 1; j <= last; j++) {
            Point point = points[j % CONTOUR_POINTS];
            fprintf(output, " %d %d l 1\n", (int)point.x + BEARING - left, (int)point.y);
        }
    }
    fputs("EndSplineSet\nEndChar\n\n", output);
}

int main(int argc, char **argv)
{
    if (argc != 3)
        fail("Usage", "build-gost INPUT.coffee OUTPUT.sfd");
    FILE *input = fopen(argv[1], "rb");
    if (!input)
        fail("Cannot open", argv[1]);
    Glyph glyphs[CYRILLIC_LENGTH]{};
    read_glyphs(input, glyphs);
    fclose(input);

    FILE *output = fopen(argv[2], "wb");
    if (!output)
        fail("Cannot open", argv[2]);
    fputs("SplineFontDB: 3.2\nFontName: GOSTLettering-Regular\n"
          "FullName: GOST Lettering Regular\nFamilyName: GOST Lettering\n"
          "Weight: Regular\nVersion: 1.000\n"
          "Comment: \"Based on the peko/GOST-FONT skeletal drawings.\"\n"
          "ItalicAngle: 0\nAscent: 920\nDescent: 80\n"
          "LayerCount: 2\nLayer: 0 1 \"Back\" 1\nLayer: 1 1 \"Fore\" 0\n"
          "Encoding: UnicodeFull\n"
          "HheadAscent: 920\nHheadAOffset: 0\nHheadDescent: -270\nHheadDOffset: 0\n"
          "OS2TypoAscent: 920\nOS2TypoAOffset: 0\nOS2TypoDescent: -270\nOS2TypoDOffset: 0\n"
          "OS2WinAscent: 920\nOS2WinAOffset: 0\nOS2WinDescent: 270\nOS2WinDOffset: 0\n"
          "LineGap: 0\nOS2LineGap: 0\nOS2Version: 3\nTTFWeight: 400\nTTFWidth: 5\n",
          output);
    fprintf(output, "BeginChars: 1114112 %d\n\n", RUSSIAN_LETTERS + 1);
    fputs("StartChar: .notdef\nEncoding: -1 -1 0\nWidth: 500\nFlags: W\n"
          "LayerCount: 2\nEndChar\n\n", output);
    unsigned index = 1;
    for (unsigned code = CYRILLIC; code < CYRILLIC + CYRILLIC_LENGTH; code++)
        if (is_russian(code))
            write_glyph(output, glyphs[code - CYRILLIC], code, index++);
    fputs("EndChars\nEndSplineFont\n", output);
    int failed = ferror(output);
    if (fclose(output) != 0 || failed)
        fail("Cannot write", argv[2]);
    printf("Generated SFD for %u Russian letters\n", index - 1);
    return EXIT_SUCCESS;
}
