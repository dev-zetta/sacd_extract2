#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "unity.h"
#include "xml_writer.h"

static char path[256];

void setUp(void)
{
    snprintf(path, sizeof(path), "sacd-xml-writer-%ld.xml", (long)getpid());
    unlink(path);
}

void tearDown(void)
{
    unlink(path);
}

static void test_writer_emits_well_formed_escaped_utf8_xml(void)
{
    char content[1024];
    FILE *file;
    size_t length;
    xmlTextWriterPtr writer = xmlNewTextWriterFilename(path, 0);
    TEST_ASSERT_NOT_NULL(writer);
    TEST_ASSERT_EQUAL_INT(0, xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL));
    TEST_ASSERT_EQUAL_INT(0, xmlTextWriterStartElement(writer, BAD_CAST "root"));
    TEST_ASSERT_EQUAL_INT(0, xmlTextWriterWriteAttribute(writer, BAD_CAST "title",
                                                         BAD_CAST "A&B \"test\""));
    TEST_ASSERT_EQUAL_INT(0, xmlTextWriterWriteElement(writer, BAD_CAST "artist",
                                                       BAD_CAST "Björk <live>"));
    TEST_ASSERT_EQUAL_INT(0, xmlTextWriterEndElement(writer));
    TEST_ASSERT_EQUAL_INT(0, xmlTextWriterEndDocument(writer));
    xmlFreeTextWriter(writer);

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    content[length] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(content, "title=\"A&amp;B &quot;test&quot;\""));
    TEST_ASSERT_NOT_NULL(strstr(content, "<artist>Björk &lt;live&gt;</artist>"));
    TEST_ASSERT_NOT_NULL(strstr(content, "</root>"));
}

static void test_writer_rejects_invalid_comment_and_depth_underflow(void)
{
    xmlTextWriterPtr writer = xmlNewTextWriterFilename(path, 0);
    TEST_ASSERT_NOT_NULL(writer);
    TEST_ASSERT_EQUAL_INT(-1, xmlTextWriterWriteComment(writer, BAD_CAST "invalid--comment"));
    TEST_ASSERT_EQUAL_INT(-1, xmlTextWriterEndElement(writer));
    xmlFreeTextWriter(writer);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_writer_emits_well_formed_escaped_utf8_xml);
    RUN_TEST(test_writer_rejects_invalid_comment_and_depth_underflow);
    return UNITY_END();
}
