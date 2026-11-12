#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/hash.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "home-terminal.h"
#include "home-welcome.h"

static const char *const orders[] = {
    "en-ru-zh", "en-zh-ru", "ru-en-zh", "ru-zh-en", "zh-en-ru", "zh-ru-en"
};

static void collect_ids(xmlNodePtr node, xmlHashTablePtr ids, const char *prefix)
{
    if (node->type != XML_ELEMENT_NODE)
        return;
    xmlChar *id = xmlGetProp(node, BAD_CAST "id");
    if (id) {
        xmlChar *value = xmlStrcat(xmlStrdup(BAD_CAST prefix), id);
        if (!value || xmlHashAddEntry(ids, id, value) != 0)
            fail("Cannot register SVG ID", (const char *)id);
        xmlFree(id);
    }
    for (xmlNodePtr child = node->children; child; child = child->next)
        collect_ids(child, ids, prefix);
}

static void free_id(void *value, const xmlChar *key)
{
    (void)key;
    xmlFree(value);
}

static void append(xmlBufferPtr buffer, const char *text, size_t length)
{
    if (xmlBufferAdd(buffer, BAD_CAST text, (int)length) != 0)
        fail("Cannot write", "XML buffer");
}

static void append_id(xmlBufferPtr buffer, xmlHashTablePtr ids, const char *start, size_t length)
{
    xmlChar *id = xmlStrndup(BAD_CAST start, (int)length);
    if (!id)
        fail("Cannot allocate", "SVG ID");
    const char *mapped = static_cast<const char *>(xmlHashLookup(ids, id));
    append(buffer, mapped ? mapped : start, mapped ? strlen(mapped) : length);
    xmlFree(id);
}

static int starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

/* SMIL uses semicolon-separated times and references such as id.end+0.5s. */
static void rewrite_timing(xmlBufferPtr buffer, xmlHashTablePtr ids, const char *value)
{
    while (*value) {
        size_t length = strcspn(value, ";");
        size_t indent = strspn(value, " \t\r\n");
        const char *token = value + indent;
        const char *dot = static_cast<const char *>(memchr(token, '.', length - indent));
        if (dot && (starts_with(dot, ".click") || starts_with(dot, ".begin") ||
                    starts_with(dot, ".end"))) {
            append(buffer, value, indent);
            size_t id_length = (size_t)(dot - token);
            if (starts_with(token, "randomPage.click"))
                append(buffer, "sharedWelcome", sizeof "sharedWelcome" - 1);
            else
                append_id(buffer, ids, token, id_length);
            append(buffer, dot, length - indent - id_length);
        } else {
            append(buffer, value, length);
        }
        value += length;
        if (*value)
            append(buffer, value++, 1);
    }
}

static void rewrite_urls(xmlBufferPtr buffer, xmlHashTablePtr ids, const char *value)
{
    const char *start;
    while ((start = strstr(value, "url(#"))) {
        start += sizeof "url(#" - 1;
        const char *end = strchr(start, ')');
        if (!end)
            break;
        append(buffer, value, (size_t)(start - value));
        append_id(buffer, ids, start, (size_t)(end - start));
        value = end;
    }
    append(buffer, value, strlen(value));
}

static void rewrite_nodes(xmlNodePtr node, xmlHashTablePtr ids, xmlBufferPtr buffer)
{
    if (node->type != XML_ELEMENT_NODE)
        return;
    const char origin[] = "https://forgetwolf.net/";
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
        xmlChar *content = xmlNodeListGetString(node->doc, attr->children, 1);
        if (!content)
            continue;
        const char *value = (const char *)content;
        xmlBufferEmpty(buffer);
        if (xmlStrEqual(attr->name, BAD_CAST "id")) {
            append_id(buffer, ids, value, strlen(value));
        } else if (xmlStrEqual(attr->name, BAD_CAST "begin") ||
                   xmlStrEqual(attr->name, BAD_CAST "end")) {
            rewrite_timing(buffer, ids, value);
        } else if (xmlStrEqual(attr->name, BAD_CAST "href") && value[0] == '#') {
            append(buffer, "#", 1);
            append_id(buffer, ids, value + 1, strlen(value + 1));
        } else if (xmlStrEqual(attr->name, BAD_CAST "href") && starts_with(value, origin)) {
            const char *route = value + sizeof origin - 1;
            append(buffer, "./", 2);
            append(buffer, route, strlen(route));
            if (*route)
                append(buffer, "/", 1);
        } else if (strstr(value, "url(#")) {
            rewrite_urls(buffer, ids, value);
        } else {
            xmlFree(content);
            continue;
        }
        if (!xmlSetNsProp(node, attr->ns, attr->name, xmlBufferContent(buffer)))
            fail("Cannot set attribute", (const char *)attr->name);
        xmlFree(content);
    }
    for (xmlNodePtr child = node->children; child; child = child->next)
        rewrite_nodes(child, ids, buffer);
}

static void prefix_ids(xmlNodePtr root, const char *prefix, const char *scene_id)
{
    xmlHashTablePtr ids = xmlHashCreate(0);
    xmlBufferPtr buffer = xmlBufferCreate();
    if (!ids || !buffer)
        fail("Cannot allocate", "SVG ID table");
    collect_ids(root, ids, prefix);
    if (xmlHashUpdateEntry(ids, BAD_CAST "randomScene", xmlStrdup(BAD_CAST scene_id), free_id) != 0)
        fail("Cannot register scene ID", scene_id);
    rewrite_nodes(root, ids, buffer);
    xmlBufferFree(buffer);
    xmlHashFree(ids, free_id);
}

static void build_welcome(xmlNodePtr group, const Welcome languages[3], const char *order,
                          double total, const char *finish, int mobile)
{
    prop(child(group, "set"), "begin", finish);
    double offset = 0;
    for (unsigned slot = 0; slot < 3; slot++) {
        const char *code = order + slot * 3;
        for (unsigned i = 0; i < 3; i++) {
            const Welcome *language = &languages[i];
            if (strncmp(code, language->language, 2) != 0)
                continue;
            xmlNodePtr svg = welcome_render(language, group, offset, total, mobile);
            char prefix[32];
            snprintf(prefix, sizeof prefix, "carousel_%s_", language->language);
            prefix_ids(svg, prefix, "randomScene");
            offset += language->length;
            break;
        }
    }
}

static void write_layout(FILE *html, const char *layout, const Welcome languages[3],
                         const TerminalTrace *trace, int mobile)
{
    char path[64];
    snprintf(path, sizeof path, "builder/templates/home-%s.svg", layout);
    xmlDocPtr doc = read_xml(path);
    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr first = child(root, "g");
    xmlNodePtr welcome_template = xmlDocCopyNode(first, doc, 1);
    if (!welcome_template)
        fail("Cannot copy", "welcome template");
    double total = languages[0].length + languages[1].length + languages[2].length;
    char time[64], finish[128];
    decimal(time, total);
    snprintf(finish, sizeof finish, "%ss;randomScene.click;randomPage.click", time);
    prop(root, "data-order", "en,ru,zh");
    prop(root, "data-duration", time);
    prop(require_id(root, "landingEnter"), "begin", finish);
    build_welcome(first, languages, orders[0], total, finish, mobile);
    terminal_render(root, trace, mobile);
    xmlNodePtr stage = require_id(root, "terminal-stage");
    xmlSetProp(stage, BAD_CAST "class", BAD_CAST "terminal-stage");
    xmlNodePtr position = stage->parent;
    xmlNodePtr fit = xmlNewNode(root->ns, BAD_CAST "g");
    xmlSetProp(fit, BAD_CAST "class", BAD_CAST "terminal-fit");
    xmlAddPrevSibling(position, fit);
    xmlUnlinkNode(position);
    xmlAddChild(fit, position);

    char prefix[32], scene_id[32];
    snprintf(scene_id, sizeof scene_id, "p0%s_randomScene", layout);
    snprintf(prefix, sizeof prefix, "p0%s_", layout);
    prefix_ids(root, prefix, scene_id);
    xmlSetProp(root, BAD_CAST "height", BAD_CAST "100%");

    xmlNodePtr anchor = first;
    for (unsigned i = 0; i < sizeof orders / sizeof orders[0]; i++) {
        xmlNodePtr wrapper = xmlNewNode(root->ns, BAD_CAST "g");
        char classes[48];
        snprintf(classes, sizeof classes, "welcome-order welcome-%u", i);
        xmlSetProp(wrapper, BAD_CAST "class", BAD_CAST classes);
        xmlSetProp(wrapper, BAD_CAST "data-order", BAD_CAST orders[i]);
        xmlAddNextSibling(anchor, wrapper);
        anchor = wrapper;
        xmlNodePtr welcome = first;
        if (i == 0) {
            xmlUnlinkNode(first);
        } else {
            welcome = xmlDocCopyNode(welcome_template, doc, 1);
            if (!welcome)
                fail("Cannot copy welcome group", orders[i]);
            build_welcome(welcome, languages, orders[i], total, finish, mobile);
            snprintf(prefix, sizeof prefix, "p%u%s_", i, layout);
            prefix_ids(welcome, prefix, scene_id);
        }
        xmlAddChild(wrapper, welcome);
    }
    xmlFreeNode(welcome_template);

    xmlBufferPtr output = xmlBufferCreate();
    if (!output || xmlNodeDump(output, doc, root, 0, 0) < 0)
        fail("Cannot serialize SVG", layout);
    fprintf(html, "<div class=\"%s\">%s</div>", layout, (const char *)xmlBufferContent(output));
    xmlBufferFree(output);
    xmlFreeDoc(doc);
}

int main(int argc, char **argv)
{
    if (argc != 2)
        fail("Usage", "pack-home OUTPUT_HTML");
    FILE *html = fopen(argv[1], "wb");
    if (!html)
        fail("Cannot open output", argv[1]);
    fputs("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
          "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
          "<meta name=\"color-scheme\" content=\"light dark\"><title>ForgetWolf</title>"
          "<link rel=\"icon\" type=\"image/x-icon\" href=\"favicon.ico\">"
          "<link rel=\"stylesheet\" href=\"site.css\"><body id=\"sharedWelcome\">"
          "<main class=\"viewport\">", html);
    Welcome languages[] = {welcome_load("en", "greeting.hpp"),
                           welcome_load("ru", "russian.hpp"),
                           welcome_load("zh", "chinese.hpp")};
    TerminalTrace *trace = terminal_execute();
    write_layout(html, "wide", languages, trace, 0);
    write_layout(html, "narrow", languages, trace, 1);
    terminal_free(trace);
    for (unsigned i = 0; i < sizeof languages / sizeof languages[0]; i++)
        xmlFreeDoc(languages[i].document);
    fputs("</main></body></html>", html);
    int write_failed = ferror(html);
    if (fclose(html) != 0 || write_failed)
        fail("Cannot write output", argv[1]);
    return EXIT_SUCCESS;
}
