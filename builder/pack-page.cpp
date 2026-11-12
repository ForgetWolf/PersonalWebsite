#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmark-gfm.h>
#include <cmark-gfm-extension_api.h>
#include <cmark-gfm-core-extensions.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <libxml/xmlregexp.h>

static void fail(const char *message, const char *detail)
{
    fprintf(stderr, "%s: %s\n", message, detail);
    exit(EXIT_FAILURE);
}

static char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0)
        fail("Cannot read", path);
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
        fail("Cannot read", path);
    char *text = static_cast<char *>(malloc((size_t)size + 1));
    if (!text || fread(text, 1, (size_t)size, file) != (size_t)size)
        fail("Cannot read", path);
    fclose(file);
    text[size] = '\0';
    return text;
}

static char *trim(char *text)
{
    while (isspace((unsigned char)*text))
        text++;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
    return text;
}

static char *next_line(char *line)
{
    char *end = strchr(line, '\n');
    if (!end)
        return NULL;
    *end = '\0';
    return end + 1;
}

static char *read_front_matter(char *text, const char **description, const char **date)
{
    if (strncmp(text, "\xef\xbb\xbf", 3) == 0)
        text += 3;
    size_t first_length = strcspn(text, "\n");
    char *first = (char *)xmlStrndup(BAD_CAST text, (int)first_length);
    if (!first)
        fail("Cannot allocate", "front matter");
    int has_metadata = strcmp(trim(first), "---") == 0;
    xmlFree(first);
    if (!has_metadata)
        return text;

    const char *legacy_date = "";
    for (char *line = next_line(text); line;) {
        char *next = next_line(line);
        if (strcmp(trim(line), "---") == 0) {
            if (!*date)
                *date = legacy_date;
            return next ? next : line + strlen(line);
        }
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = trim(line);
            char *value = trim(colon + 1);
            size_t length = strlen(value);
            if (length >= 4 && strncmp(value, "{{", 2) == 0 &&
                strcmp(value + length - 2, "}}") == 0) {
                value[length - 2] = '\0';
                value = trim(value + 2);
            }
            if (strcmp(key, "desc") == 0)
                *description = value;
            else if (strcmp(key, "date") == 0)
                *date = value;
            else if (strcmp(key, "data") == 0)
                legacy_date = value;
        }
        line = next;
    }
    fail("Unclosed", "Markdown front matter");
    return NULL;
}

static void add(xmlBufferPtr html, const char *text)
{
    if (xmlBufferCCat(html, text) != 0)
        fail("Cannot write", "friend entry");
}

static xmlChar *escape(const char *text)
{
    xmlChar *escaped = xmlEncodeSpecialChars(NULL, BAD_CAST text);
    if (!escaped)
        fail("Cannot escape", "HTML");
    return escaped;
}

static cmark_node *friend_card(const char *label)
{
    xmlBufferPtr url = xmlBufferCreate();
    xmlBufferPtr html = xmlBufferCreate();
    if (!url || !html)
        fail("Cannot allocate", "friend entry");
    if (!strstr(label, "://"))
        add(url, "https://");
    add(url, label);
    const char *address = (const char *)xmlBufferContent(url);
    const char *host = strstr(address, "://") + 3;
    size_t authority_length = (size_t)(host - address) + strcspn(host, "/?#");
    xmlChar *origin = xmlStrndup(BAD_CAST address, (int)authority_length);
    if (!origin)
        fail("Cannot allocate", "friend URL");
    xmlURIPtr uri = xmlParseURI((const char *)origin);
    xmlFree(origin);
    if (!uri || !uri->server)
        fail("Invalid friend URL", label);
    for (char *letter = uri->server; *letter; letter++)
        *letter = (char)tolower((unsigned char)*letter);
    char initial[] = {(char)toupper((unsigned char)*uri->server), '\0'};
    int linusaur = strcmp(uri->server, "linusaur.net") == 0;
    int ymckc = strcmp(uri->server, "ymckc.monster") == 0;

    add(html, "<div class=\"friend-entry\"><div class=\"friend-mark\" aria-hidden=\"true\">");
    if (linusaur)
        add(html, "<object class=\"friend-icon\" data=\"https://linusaur.net/favicon.ico\" type=\"image/x-icon\">");
    if (ymckc)
        add(html, "<object class=\"friend-icon\" data=\"https://ymckc.monster/favicon.ico\" type=\"image/gif\">");
    add(html, "<span class=\"friend-initial\">");
    add(html, initial);
    add(html, "</span>");
    if (linusaur || ymckc)
        add(html, "</object>");
    add(html, "</div><a href=\"");
    xmlChar *escaped_url = escape(address);
    xmlChar *escaped_label = escape(label);
    add(html, (const char *)escaped_url);
    add(html, "\">");
    add(html, (const char *)escaped_label);
    add(html, "</a></div>\n");

    cmark_node *card = cmark_node_new(CMARK_NODE_CUSTOM_BLOCK);
    if (!card || !cmark_node_set_on_enter(card, (const char *)xmlBufferContent(html)))
        fail("Cannot create", "friend entry");
    xmlFree(escaped_url);
    xmlFree(escaped_label);
    xmlFreeURI(uri);
    xmlBufferFree(url);
    xmlBufferFree(html);
    return card;
}

static void add_friend_cards(cmark_node *document, char *body)
{
    xmlRegexpPtr url = xmlRegexpCompile(BAD_CAST
        "([Hh][Tt][Tt][Pp][Ss]?://)?([A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?\\.)+"
        "[A-Za-z]{2,}(:[0-9]+)?([/?#][^\\s<>]*)?");
    if (!url)
        fail("Cannot compile", "friend URL pattern");
    int line_number = 1;
    char *line = body;
    char *next_line_start = next_line(line);
    for (cmark_node *node = cmark_node_first_child(document), *next; node; node = next) {
        next = cmark_node_next(node);
        if (cmark_node_get_type(node) != CMARK_NODE_PARAGRAPH)
            continue;
        int end = cmark_node_get_end_line(node);
        while (line_number < end && next_line_start) {
            line = next_line_start;
            next_line_start = next_line(line);
            line_number++;
        }
        char *label = trim(line);
        if (line_number != end || xmlRegexpExec(url, BAD_CAST label) != 1)
            continue;

        cmark_node *tail = cmark_node_last_child(node);
        while (tail && cmark_node_get_type(tail) != CMARK_NODE_SOFTBREAK &&
               cmark_node_get_type(tail) != CMARK_NODE_LINEBREAK)
            tail = cmark_node_previous(tail);
        /* Keep a multi-line paragraph when the last source line is inside a span. */
        if (!tail && cmark_node_get_start_line(node) != end)
            continue;
        if (!cmark_node_insert_after(node, friend_card(label)))
            fail("Cannot insert", "friend entry");
        if (!tail) {
            cmark_node_free(node);
            continue;
        }
        while (cmark_node_next(tail))
            cmark_node_free(cmark_node_next(tail));
        cmark_node_free(tail);
    }
    xmlRegFreeRegexp(url);
}

static char *render_markdown(char *body, int friends)
{
    const int options = CMARK_OPT_HARDBREAKS | CMARK_OPT_FOOTNOTES |
                        CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE | CMARK_OPT_VALIDATE_UTF8;
    const char *const extensions[] = {"table", "strikethrough"};
    cmark_gfm_core_extensions_ensure_registered();
    cmark_parser *parser = cmark_parser_new(options);
    if (!parser)
        fail("Cannot allocate", "Markdown parser");
    for (size_t i = 0; i < sizeof extensions / sizeof extensions[0]; i++) {
        cmark_syntax_extension *extension = cmark_find_syntax_extension(extensions[i]);
        if (!extension || !cmark_parser_attach_syntax_extension(parser, extension))
            fail("Cannot enable Markdown extension", extensions[i]);
    }
    cmark_parser_feed(parser, body, strlen(body));
    cmark_node *document = cmark_parser_finish(parser);
    if (!document)
        fail("Cannot parse", "Markdown");
    if (friends)
        add_friend_cards(document, body);
    char *html = cmark_render_html(document, options, cmark_parser_get_syntax_extensions(parser));
    cmark_node_free(document);
    cmark_parser_free(parser);
    if (!html)
        fail("Cannot render", "Markdown");
    size_t length = strlen(html);
    if (length && html[length - 1] == '\n')
        html[length - 1] = '\0';
    return html;
}

static void write_page(const char *path, const char *markup,
                       const char *description, const char *date, const char *body)
{
    const char *const markers[] = {"@@DESC@@", "@@DATE@@", "@@BODY@@"};
    const char *const values[] = {description, date, body};
    FILE *file = fopen(path, "wb");
    if (!file)
        fail("Cannot write", path);
    for (size_t i = 0; i < sizeof markers / sizeof markers[0]; i++) {
        const char *marker = strstr(markup, markers[i]);
        if (!marker)
            fail("Missing template marker", markers[i]);
        fwrite(markup, 1, (size_t)(marker - markup), file);
        fputs(values[i], file);
        markup = marker + strlen(markers[i]);
    }
    fputs(markup, file);
    int write_failed = ferror(file);
    if (fclose(file) != 0 || write_failed)
        fail("Cannot write", path);
}

int main(int argc, char **argv)
{
    if (argc != 5)
        fail("Usage", "pack-page PAGE INPUT_MD TEMPLATE OUTPUT_HTML");
    char *source = read_text(argv[2]);
    char *markup = read_text(argv[3]);
    const char *description = "", *date = NULL;
    char *body = read_front_matter(source, &description, &date);
    xmlChar *escaped_description = escape(description);
    xmlChar *escaped_date = escape(date ? date : "");
    char *html = render_markdown(body, strcmp(argv[1], "friends") == 0);
    write_page(argv[4], markup, (const char *)escaped_description, (const char *)escaped_date, html);
    free(html);
    xmlFree(escaped_description);
    xmlFree(escaped_date);
    free(markup);
    free(source);
    return EXIT_SUCCESS;
}
