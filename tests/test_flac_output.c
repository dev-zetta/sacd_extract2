#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scarletbook_output.h"

extern scarletbook_format_handler_t const *flac_format_fn(void);

void setUp(void) {}
void tearDown(void) {}

static void test_flac_handler_writes_a_valid_native_flac_stream(void)
{
    scarletbook_output_format_t format;
    const scarletbook_format_handler_t *handler = flac_format_fn();
    uint8_t dsd[4096];
    unsigned char magic[4];

    memset(&format, 0, sizeof(format));
    memset(dsd, 0xaa, sizeof(dsd));
    format.fd = tmpfile();
    TEST_ASSERT_NOT_NULL(format.fd);
    format.channel_count = 2;
    format.flac_sample_rate = 88200;
    format.handler = *handler;
    format.priv = calloc(1, handler->priv_size);
    TEST_ASSERT_NOT_NULL(format.priv);

    TEST_ASSERT_EQUAL_INT(0, handler->startwrite(&format));
    TEST_ASSERT_EQUAL_INT((int)sizeof(dsd),
                          handler->write(&format, dsd, sizeof(dsd)));
    TEST_ASSERT_EQUAL_INT(0, handler->stopwrite(&format));
    TEST_ASSERT_EQUAL_INT(0, fflush(format.fd));
    TEST_ASSERT_EQUAL_INT(0, fseek(format.fd, 0, SEEK_SET));
    TEST_ASSERT_EQUAL_UINT(4, fread(magic, 1, sizeof(magic), format.fd));
    TEST_ASSERT_EQUAL_MEMORY("fLaC", magic, sizeof(magic));

    free(format.priv);
    fclose(format.fd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flac_handler_writes_a_valid_native_flac_stream);
    return UNITY_END();
}
