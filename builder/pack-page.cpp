#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <string>
#include <string_view>

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
    if (!uri || !uri->server)
        fail("Invalid friend URL", label);
    for (char *letter = uri->server; *letter; letter++)
        *letter = (char)tolower((unsigned char)*letter);
    char initial[] = {(char)toupper((unsigned char)*uri->server), '\0'};
    const char *local_icon = nullptr;
    if (strcmp(uri->server, "azote.top") == 0)
        local_icon = "azote";
    else if (strcmp(uri->server, "blog.struggle.work") == 0)
        local_icon = "watchdogexd";
    else if (strcmp(uri->server, "r18.forgetwolf.net") == 0)
        local_icon = "forgetwolf-r18";
    else if (strcmp(uri->server, "linusaur.net") == 0)
        local_icon = "linusaur";
    else if (strcmp(uri->server, "ymckc.monster") == 0)
        local_icon = "ymckc";

    add(html, "<div class=\"friend-entry\"><div class=\"friend-mark\" aria-hidden=\"true\">");
    if (local_icon) {
        add(html, "<picture><source media=\"(prefers-color-scheme: dark)\" srcset=\"../favicon/");
        add(html, local_icon);
        add(html, "-dark.png\"><img class=\"friend-icon\" src=\"../favicon/");
        add(html, local_icon);
        add(html, "-light.png\" alt=\"");
        add(html, initial);
        add(html, "\"></picture>");
    } else {
        xmlChar *escaped_origin = escape((const char *)origin);
        add(html, "<object class=\"friend-icon\" data=\"");
        add(html, (const char *)escaped_origin);
        add(html, "/favicon.ico\" type=\"image/x-icon\"><span class=\"friend-initial\">");
        add(html, initial);
        add(html, "</span></object>");
        xmlFree(escaped_origin);
    }
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
    xmlFree(origin);
    xmlFree(escaped_url);
    xmlFree(escaped_label);
    xmlFreeURI(uri);
    xmlBufferFree(url);
    xmlBufferFree(html);
    return card;
}

struct CoverFriend {
    std::string name;
    std::string label;
    std::string url;
    std::string_view icon;
    std::string_view extension;
    std::string_view color;
};

using CoverFriends = std::array<CoverFriend, 4>;

static void record_cover_friend(CoverFriends &friends, const char *address,
                                cmark_node *description, int options, cmark_llist *extensions)
{
    const std::string absolute = strstr(address, "://") ? address : std::string("https://") + address;
    xmlURIPtr uri = xmlParseURI(absolute.c_str());
    if (!uri)
        return;
    const struct {
        const char *host, *icon, *extension, *color;
    } covers[] = {
        {"azote.top", "azote-pixel", "png", "#e2535b"},
        {"r18.forgetwolf.net", "forgetwolf-r18", "png", "#22a699"},
        {"linusaur.net", "linusaur", "svg", "#5b8fe8"},
        {"ymckc.monster", "ymckc", "png", "#d3ab26"},
    };
    const auto *cover = covers;
    for (; cover != covers + friends.size(); ++cover)
        if (uri->server && xmlStrcasecmp(BAD_CAST uri->server, BAD_CAST cover->host) == 0)
            break;
    xmlFreeURI(uri);
    if (cover == covers + friends.size())
        return;

    size_t index = 0;
    while (index < friends.size() && !friends[index].url.empty()) {
        if (friends[index].icon == cover->icon)
            return;
        ++index;
    }
    if (index == friends.size())
        return;
    CoverFriend &entry = friends[index];
    entry.icon = cover->icon;
    entry.extension = cover->extension;
    entry.color = cover->color;
    xmlChar *escaped_url = escape(absolute.c_str());
    entry.url = reinterpret_cast<const char *>(escaped_url);
    xmlFree(escaped_url);
    std::string label = address;
    if (description) {
        char *rendered = cmark_render_html(description, options, extensions);
        if (!rendered)
            fail("Cannot render", "friend name");
        std::string_view markup(rendered);
        if (markup.size() >= 8 && markup.substr(0, 3) == "<p>" &&
            markup.substr(markup.size() - 5) == "</p>\n")
            entry.name = markup.substr(3, markup.size() - 8);
        free(rendered);

        cmark_iter *iter = cmark_iter_new(description);
        if (!iter)
            fail("Cannot read", "friend name");
        label.clear();
        for (auto event = cmark_iter_next(iter); event != CMARK_EVENT_DONE; event = cmark_iter_next(iter)) {
            if (event != CMARK_EVENT_ENTER)
                continue;
            cmark_node *node = cmark_iter_get_node(iter);
            const auto type = cmark_node_get_type(node);
            if (type == CMARK_NODE_TEXT || type == CMARK_NODE_CODE)
                label += cmark_node_get_literal(node);
            else if (type == CMARK_NODE_SOFTBREAK || type == CMARK_NODE_LINEBREAK)
                label += ' ';
        }
        cmark_iter_free(iter);
        if (label.empty())
            label = address;
    }
    xmlChar *escaped_label = escape(label.c_str());
    entry.label = reinterpret_cast<const char *>(escaped_label);
    if (entry.name.empty())
        entry.name = entry.label;
    xmlFree(escaped_label);
}

static bool add_friend_cards(cmark_node *document, char *body, CoverFriends &friends,
                             int options, cmark_llist *extensions)
{
    xmlRegexpPtr url = xmlRegexpCompile(BAD_CAST
        "([Hh][Tt][Tt][Pp][Ss]?://)?([A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?\\.)+"
        "[A-Za-z]{2,}(:[0-9]+)?([/?#][^\\s<>]*)?");
    if (!url)
        fail("Cannot compile", "friend URL pattern");
    int line_number = 1;
    char *line = body;
    char *next_line_start = next_line(line);
    size_t cards = 0;
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
        cmark_node *card = cmark_node_new(CMARK_NODE_CUSTOM_BLOCK);
        if (!card || !cmark_node_set_on_enter(card, "<div class=\"friend-card\">") ||
            !cmark_node_set_on_exit(card, "</div>") || !cmark_node_insert_after(node, card))
            fail("Cannot insert", "friend card");
        if (!tail) {
            cmark_node_free(node);
        } else {
            while (cmark_node_next(tail))
                cmark_node_free(cmark_node_next(tail));
            cmark_node_free(tail);
            cmark_node_unlink(node);
            if (!cmark_node_append_child(card, node))
                fail("Cannot insert", "friend description");
        }
        if (!cmark_node_append_child(card, friend_card(label)))
            fail("Cannot insert", "friend link");
        record_cover_friend(friends, label, tail ? node : nullptr, options, extensions);
        ++cards;
    }
    xmlRegFreeRegexp(url);
    size_t blocks = 0;
    for (auto *node = cmark_node_first_child(document); node; node = cmark_node_next(node))
        ++blocks;
    if (cards != friends.size() || blocks != cards)
        return false;
    for (const auto &entry : friends)
        if (entry.url.empty())
            return false;
    return true;
}

static char *render_markdown(char *body, CoverFriends *friends, bool &use_coverflow)
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
        use_coverflow = add_friend_cards(document, body, *friends, options,
                                         cmark_parser_get_syntax_extensions(parser));
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

static std::string render_coverflow(std::string_view markup, const CoverFriends &friends)
{
    std::string output;
    while (!markup.empty()) {
        const auto start = markup.find("@@");
        output += markup.substr(0, start);
        if (start == std::string_view::npos)
            break;
        const auto end = markup.find("@@", start + 2);
        if (end == std::string_view::npos)
            fail("Unclosed", "friend template marker");
        const auto marker = markup.substr(start + 2, end - start - 2);
        const auto index = static_cast<size_t>(marker.empty() ? -1 : marker.back() - '0');
        if (index >= friends.size())
            fail("Invalid", "friend template marker");
        const auto key = marker.substr(0, marker.size() - 1);
        const auto &entry = friends[index];
        if (key == "FRIEND_NAME_")
            output += entry.name;
        else if (key == "FRIEND_LABEL_")
            output += entry.label;
        else if (key == "FRIEND_URL_")
            output += entry.url;
        else if (key == "FRIEND_COLOR_")
            output += entry.color;
        else if (key == "FRIEND_ICON_DARK_" || key == "FRIEND_ICON_LIGHT_") {
            output += entry.icon;
            output += key == "FRIEND_ICON_DARK_" ? "-dark." : "-light.";
            output += entry.extension;
        }
        else
            fail("Unknown", "friend template marker");
        markup.remove_prefix(end + 2);
    }
    return output;
}

static void write_page(const char *path, const char *markup,
                       const char *description, const char *date, const char *body,
                       const char *friend_style)
{
    const char *style_marker = "@@FRIEND_STYLE@@";
    const char *const markers[] = {"@@DESC@@", "@@DATE@@", "@@BODY@@"};
    const char *const values[] = {description, date, body};
    FILE *file = fopen(path, "wb");
    if (!file)
        fail("Cannot write", path);
    if (const char *style = strstr(markup, style_marker)) {
        fwrite(markup, 1, static_cast<size_t>(style - markup), file);
        fputs(friend_style, file);
        markup = style + strlen(style_marker);
    }
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
    if (argc != 5 && argc != 6)
        fail("Usage", "pack-page PAGE INPUT_MD TEMPLATE OUTPUT_HTML [COVERFLOW_TEMPLATE]");
    char *source = read_text(argv[2]);
    char *markup = read_text(argv[3]);
    const char *description = "", *date = NULL;
    char *body = read_front_matter(source, &description, &date);
    xmlChar *escaped_description = escape(description);
    xmlChar *escaped_date = escape(date ? date : "");
    const bool is_friends = strcmp(argv[1], "friends") == 0;
    CoverFriends friends;
    bool use_coverflow = false;
    char *html = render_markdown(body, is_friends ? &friends : nullptr, use_coverflow);
    std::string content = html;
    const char *friend_style = "";
    if (use_coverflow && argc == 6) {
        char *cover_template = read_text(argv[5]);
        content = render_coverflow(cover_template, friends);
        free(cover_template);
        friend_style = "<link rel=\"stylesheet\" href=\"../coverflow.css\">";
    } else if (is_friends) {
        content = "<main class=\"friend-list\">" + content + "</main>";
    }
    write_page(argv[4], markup, (const char *)escaped_description, (const char *)escaped_date,
               content.c_str(), friend_style);
    free(html);
    xmlFree(escaped_description);
    xmlFree(escaped_date);
    free(markup);
    free(source);
    return EXIT_SUCCESS;
}
