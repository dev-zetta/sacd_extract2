#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "scarletbook.h"
#include "scarletbook_read.h"

static void ignore_frame(scarletbook_handle_t *handle, uint8_t *data, size_t size, void *userdata)
{
    (void)handle; (void)data; (void)size; (void)userdata;
}

void setUp(void) {}
void tearDown(void) {}

static void test_malformed_packet_is_counted_and_next_sector_is_processed(void)
{
    scarletbook_handle_t handle;
    uint8_t sectors[2 * SACD_LSN_SIZE];
    memset(&handle, 0, sizeof(handle));
    memset(sectors, 0, sizeof(sectors));
    handle.frame.data = malloc(MAX_DST_SIZE);
    TEST_ASSERT_NOT_NULL(handle.frame.data);

    /* Little-endian audio header: one packet; packet length 2047 > 2045. */
    sectors[0] = 0x20;
    sectors[1] = 0x07;
    sectors[2] = 0xff;
    TEST_ASSERT_EQUAL_INT(-1, scarletbook_process_frames(&handle, sectors, 2, 1,
                                                         ignore_frame, NULL));
    TEST_ASSERT_FALSE(handle.frame.started);
    free(handle.frame.data);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_malformed_packet_is_counted_and_next_sector_is_processed);
    return UNITY_END();
}
