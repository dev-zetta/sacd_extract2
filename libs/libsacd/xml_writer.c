#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xml_writer.h"

#define XML_WRITER_MAX_DEPTH 64

struct sacd_xml_writer
{
    FILE *file;
    char *elements[XML_WRITER_MAX_DEPTH];
    unsigned int depth;
    int start_tag_open;
    int failed;
};

static int write_bytes(xmlTextWriterPtr writer, const char *text)
{
    if (!writer || writer->failed || fputs(text, writer->file) == EOF)
    {
        if (writer)
            writer->failed = 1;
        return -1;
    }
    return 0;
}

static int write_indent(xmlTextWriterPtr writer, unsigned int depth)
{
    for (unsigned int i = 0; i < depth; ++i)
        if (write_bytes(writer, "  ") < 0)
            return -1;
    return 0;
}

static int close_start_tag(xmlTextWriterPtr writer)
{
    if (writer->start_tag_open)
    {
        writer->start_tag_open = 0;
        return write_bytes(writer, ">\n");
    }
    return 0;
}

static int write_escaped(xmlTextWriterPtr writer, const char *value, int attribute)
{
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    while (*cursor)
    {
        const char *entity = NULL;
        switch (*cursor)
        {
        case '&': entity = "&amp;"; break;
        case '<': entity = "&lt;"; break;
        case '>': entity = "&gt;"; break;
        case '"': if (attribute) entity = "&quot;"; break;
        case '\'': if (attribute) entity = "&apos;"; break;
        default: break;
        }
        if (entity)
        {
            if (write_bytes(writer, entity) < 0)
                return -1;
        }
        else if (*cursor >= 0x20 || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
        {
            if (fputc(*cursor, writer->file) == EOF)
            {
                writer->failed = 1;
                return -1;
            }
        }
        cursor++;
    }
    return 0;
}

xmlTextWriterPtr xmlNewTextWriterFilename(const char *path, int compression)
{
    xmlTextWriterPtr writer;
    (void)compression;
    if (!path)
        return NULL;
    writer = calloc(1, sizeof(*writer));
    if (!writer)
        return NULL;
    writer->file = fopen(path, "wb");
    if (!writer->file)
    {
        free(writer);
        return NULL;
    }
    return writer;
}

int xmlTextWriterStartDocument(xmlTextWriterPtr writer, const char *version,
                               const char *encoding, const char *standalone)
{
    (void)standalone;
    if (!writer)
        return -1;
    return fprintf(writer->file, "<?xml version=\"%s\" encoding=\"%s\"?>\n",
                   version ? version : "1.0", encoding ? encoding : "UTF-8") < 0 ? -1 : 0;
}

int xmlTextWriterStartElement(xmlTextWriterPtr writer, const xmlChar *name)
{
    const char *element = (const char *)name;
    if (!writer || !element || writer->depth >= XML_WRITER_MAX_DEPTH ||
        close_start_tag(writer) < 0 || write_indent(writer, writer->depth) < 0)
        return -1;
    writer->elements[writer->depth] = strdup(element);
    if (!writer->elements[writer->depth])
        return -1;
    writer->depth++;
    writer->start_tag_open = 1;
    return fprintf(writer->file, "<%s", element) < 0 ? -1 : 0;
}

int xmlTextWriterWriteAttribute(xmlTextWriterPtr writer, const xmlChar *name,
                                const xmlChar *value)
{
    if (!writer || !writer->start_tag_open || !name ||
        fprintf(writer->file, " %s=\"", (const char *)name) < 0 ||
        write_escaped(writer, (const char *)value, 1) < 0)
        return -1;
    return write_bytes(writer, "\"");
}

int xmlTextWriterWriteFormatAttribute(xmlTextWriterPtr writer, const xmlChar *name,
                                      const char *format, ...)
{
    char value[1024];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(value, sizeof(value), format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(value))
        return -1;
    return xmlTextWriterWriteAttribute(writer, name, (const xmlChar *)value);
}

int xmlTextWriterWriteElement(xmlTextWriterPtr writer, const xmlChar *name,
                              const xmlChar *value)
{
    if (!writer || !name || close_start_tag(writer) < 0 ||
        write_indent(writer, writer->depth) < 0 ||
        fprintf(writer->file, "<%s>", (const char *)name) < 0 ||
        write_escaped(writer, (const char *)value, 0) < 0 ||
        fprintf(writer->file, "</%s>\n", (const char *)name) < 0)
        return -1;
    return 0;
}

int xmlTextWriterWriteFormatElement(xmlTextWriterPtr writer, const xmlChar *name,
                                    const char *format, ...)
{
    char value[1024];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(value, sizeof(value), format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(value))
        return -1;
    return xmlTextWriterWriteElement(writer, name, (const xmlChar *)value);
}

int xmlTextWriterWriteComment(xmlTextWriterPtr writer, const xmlChar *value)
{
    const char *text = (const char *)(value ? value : (const xmlChar *)"");
    if (!writer || strstr(text, "--") || close_start_tag(writer) < 0 ||
        write_indent(writer, writer->depth) < 0 || write_bytes(writer, "<!--") < 0 ||
        write_escaped(writer, text, 0) < 0 || write_bytes(writer, "-->\n") < 0)
        return -1;
    return 0;
}

int xmlTextWriterEndElement(xmlTextWriterPtr writer)
{
    char *name;
    if (!writer || writer->depth == 0)
        return -1;
    name = writer->elements[--writer->depth];
    if (writer->start_tag_open)
    {
        writer->start_tag_open = 0;
        if (write_bytes(writer, "/>\n") < 0)
            goto error;
    }
    else if (write_indent(writer, writer->depth) < 0 ||
             fprintf(writer->file, "</%s>\n", name) < 0)
        goto error;
    free(name);
    writer->elements[writer->depth] = NULL;
    return 0;
error:
    free(name);
    writer->elements[writer->depth] = NULL;
    return -1;
}

int xmlTextWriterEndDocument(xmlTextWriterPtr writer)
{
    if (!writer)
        return -1;
    while (writer->depth > 0)
        if (xmlTextWriterEndElement(writer) < 0)
            return -1;
    return fflush(writer->file) == 0 && !writer->failed ? 0 : -1;
}

void xmlFreeTextWriter(xmlTextWriterPtr writer)
{
    if (!writer)
        return;
    while (writer->depth > 0)
        free(writer->elements[--writer->depth]);
    if (writer->file)
        fclose(writer->file);
    free(writer);
}

void xmlCleanupParser(void)
{
}
