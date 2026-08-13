#ifndef SACD_XML_WRITER_H_INCLUDED
#define SACD_XML_WRITER_H_INCLUDED

/* Minimal write-only compatibility layer for the libxml2 xmlTextWriter calls
 * used by scarletbook_xml.c. It keeps XML export dependency-free. */

typedef unsigned char xmlChar;
typedef struct sacd_xml_writer *xmlTextWriterPtr;

#define BAD_CAST (const xmlChar *)
#define LIBXML_TEST_VERSION ((void)0);

xmlTextWriterPtr xmlNewTextWriterFilename(const char *path, int compression);
int xmlTextWriterStartDocument(xmlTextWriterPtr writer, const char *version,
                               const char *encoding, const char *standalone);
int xmlTextWriterStartElement(xmlTextWriterPtr writer, const xmlChar *name);
int xmlTextWriterWriteAttribute(xmlTextWriterPtr writer, const xmlChar *name,
                                const xmlChar *value);
int xmlTextWriterWriteFormatAttribute(xmlTextWriterPtr writer, const xmlChar *name,
                                      const char *format, ...);
int xmlTextWriterWriteElement(xmlTextWriterPtr writer, const xmlChar *name,
                              const xmlChar *value);
int xmlTextWriterWriteFormatElement(xmlTextWriterPtr writer, const xmlChar *name,
                                    const char *format, ...);
int xmlTextWriterWriteComment(xmlTextWriterPtr writer, const xmlChar *value);
int xmlTextWriterEndElement(xmlTextWriterPtr writer);
int xmlTextWriterEndDocument(xmlTextWriterPtr writer);
void xmlFreeTextWriter(xmlTextWriterPtr writer);
void xmlCleanupParser(void);

#endif
