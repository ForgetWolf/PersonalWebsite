#ifndef HOME_SVG_H
#define HOME_SVG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

static inline void fail(const char *message, const char *detail)
{
    fprintf(stderr, "%s: %s\n", message, detail);
    exit(EXIT_FAILURE);
}

static inline char *read_text(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0)
        fail("Cannot read", path);
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
        fail("Cannot read", path);
    char *text = static_cast<char *>(malloc(static_cast<size_t>(length) + 1));
    if (!text || fread(text, 1, (size_t)length, file) != (size_t)length)
        fail("Cannot read", path);
    fclose(file);
    text[length] = '\0';
    return text;
}

static inline int named(xmlNodePtr node, const char *name)
{
    return node && node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, BAD_CAST name);
}

static inline xmlNodePtr child(xmlNodePtr parent, const char *name)
{
    for (xmlNodePtr node = parent->children; node; node = node->next)
        if (named(node, name))
            return node;
    fail("Missing SVG element", name);
    return NULL;
}

static inline xmlNodePtr find_id(xmlNodePtr node, const char *id)
{
    if (node->type != XML_ELEMENT_NODE)
        return NULL;
    xmlChar *value = xmlGetProp(node, BAD_CAST "id");
    int match = xmlStrEqual(value, BAD_CAST id);
    xmlFree(value);
    if (match)
        return node;
    for (xmlNodePtr item = node->children; item; item = item->next) {
        xmlNodePtr result = find_id(item, id);
        if (result)
            return result;
    }
    return NULL;
}

static inline xmlNodePtr require_id(xmlNodePtr node, const char *id)
{
    xmlNodePtr found = find_id(node, id);
    if (!found)
        fail("Missing SVG ID", id);
    return found;
}

static inline void prop(xmlNodePtr node, const char *key, const char *value)
{
    if (!xmlSetProp(node, BAD_CAST key, BAD_CAST value))
        fail("Cannot set SVG attribute", key);
}

static inline xmlNodePtr element(xmlNodePtr node, const char *name)
{
    xmlNodePtr result = xmlNewChild(node, node->ns, BAD_CAST name, NULL);
    if (!result)
        fail("Cannot create SVG element", name);
    return result;
}

static inline void buffer_add(xmlBufferPtr buffer, const char *value)
{
    if (xmlBufferCCat(buffer, value) != 0)
        fail("Cannot append", "SVG buffer");
}

static inline xmlBufferPtr new_buffer(void)
{
    xmlBufferPtr buffer = xmlBufferCreate();
    if (!buffer)
        fail("Cannot allocate", "SVG buffer");
    return buffer;
}

static inline xmlDocPtr read_xml(const char *path)
{
    xmlDocPtr document = xmlReadFile(path, NULL, XML_PARSE_NONET);
    if (!document || !xmlDocGetRootElement(document))
        fail("Cannot read XML", path);
    return document;
}

/* Keep coordinates concise. Numerical comparisons do not depend on formatting. */
static inline void decimal(char out[64], double value)
{
    snprintf(out, 64, "%.12f", value);
    char *end = out + strlen(out);
    while (end > out && end[-1] == '0')
        *--end = '\0';
    if (end > out && end[-1] == '.')
        *--end = '\0';
    if (strcmp(out, "-0") == 0)
        strcpy(out, "0");
}

static inline void numeric(xmlNodePtr node, const char *key, double value)
{
    char out[64];
    decimal(out, value);
    prop(node, key, out);
}

#endif
