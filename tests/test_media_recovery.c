#include <string.h>

#include "unity.h"
#include "media_recovery.h"

typedef struct fake_reader_s
{
    uint32_t batch_prefix;
    uint32_t permanent_hole;
    uint32_t transient_lsn;
    unsigned int transient_failures;
    unsigned int calls[32];
    int fatal;
} fake_reader_t;

static sacd_input_read_result_t result(sacd_input_status_t status, uint32_t blocks)
{
    sacd_input_read_result_t value;
    memset(&value, 0, sizeof(value));
    value.status = status;
    value.blocks_read = blocks;
    return value;
}

static sacd_input_read_result_t fake_read(void *userdata, uint32_t lsn,
                                         uint32_t blocks, uint8_t *buffer)
{
    fake_reader_t *fake = userdata;
    if (fake->fatal)
        return result(SACD_INPUT_FATAL, 0);
    if (blocks > 1)
    {
        memset(buffer, 0x11, fake->batch_prefix * 4);
        return result(fake->batch_prefix == blocks ? SACD_INPUT_COMPLETE : SACD_INPUT_SHORT,
                      fake->batch_prefix);
    }
    fake->calls[lsn]++;
    if (lsn == fake->permanent_hole)
        return result(SACD_INPUT_RETRIABLE, 0);
    if (lsn == fake->transient_lsn && fake->calls[lsn] <= fake->transient_failures)
        return result(SACD_INPUT_RETRIABLE, 0);
    memset(buffer, (int)lsn, 4);
    return result(SACD_INPUT_COMPLETE, 1);
}

static sacd_input_read_result_t all_holes_read(void *userdata, uint32_t lsn,
                                               uint32_t blocks, uint8_t *data)
{
    (void)userdata; (void)lsn; (void)blocks; (void)data;
    return result(SACD_INPUT_RETRIABLE, 0);
}

void setUp(void) {}
void tearDown(void) {}

static void test_short_read_recovers_remaining_sectors_and_retries_transient_error(void)
{
    fake_reader_t fake = {.batch_prefix = 2, .permanent_hole = UINT32_MAX,
                          .transient_lsn = 12, .transient_failures = 2};
    uint8_t buffer[16];
    uint8_t valid[4];
    const uint8_t expected_valid[4] = {1, 1, 1, 1};
    media_recovery_result_t recovery = media_read_recover(fake_read, &fake, 10, 4, 4,
                                                           buffer, valid);
    TEST_ASSERT_FALSE(recovery.fatal);
    TEST_ASSERT_EQUAL_UINT32(4, recovery.valid_blocks);
    TEST_ASSERT_EQUAL_UINT32(0, recovery.holes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_valid, valid, 4);
    TEST_ASSERT_EQUAL_UINT(3, fake.calls[12]);
}

static void test_permanent_hole_is_zero_filled_after_two_retries(void)
{
    fake_reader_t fake = {.batch_prefix = 0, .permanent_hole = 3,
                          .transient_lsn = UINT32_MAX};
    uint8_t buffer[12];
    uint8_t valid[3];
    const uint8_t expected_zero[4] = {0, 0, 0, 0};
    const uint8_t expected_valid[3] = {1, 0, 1};
    media_recovery_result_t recovery = media_read_recover(fake_read, &fake, 2, 3, 4,
                                                           buffer, valid);
    TEST_ASSERT_EQUAL_UINT32(2, recovery.valid_blocks);
    TEST_ASSERT_EQUAL_UINT32(1, recovery.holes);
    TEST_ASSERT_EQUAL_UINT(3, fake.calls[3]);
    TEST_ASSERT_EACH_EQUAL_HEX8(2, buffer, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_zero, buffer + 4, 4);
    TEST_ASSERT_EACH_EQUAL_HEX8(4, buffer + 8, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_valid, valid, 3);
}

static void test_fatal_input_is_not_retried(void)
{
    fake_reader_t fake = {.fatal = 1};
    uint8_t buffer[4];
    uint8_t valid[1];
    media_recovery_result_t recovery = media_read_recover(fake_read, &fake, 0, 1, 4,
                                                           buffer, valid);
    TEST_ASSERT_TRUE(recovery.fatal);
    TEST_ASSERT_EQUAL_UINT32(0, recovery.holes);
}

static void test_error_budget_allows_tenth_and_rejects_eleventh(void)
{
    uint32_t total;
    TEST_ASSERT_FALSE(media_error_budget_add(9, 1, 10, &total));
    TEST_ASSERT_EQUAL_UINT32(10, total);
    TEST_ASSERT_TRUE(media_error_budget_add(10, 1, 10, &total));
    TEST_ASSERT_EQUAL_UINT32(11, total);
    TEST_ASSERT_TRUE(media_error_budget_add(0, 1, 0, &total));
}

static void test_limited_recovery_stops_at_eleventh_hole(void)
{
    fake_reader_t fake = {.batch_prefix = 0, .permanent_hole = UINT32_MAX,
                          .transient_lsn = UINT32_MAX};
    uint8_t buffer[48];
    uint8_t valid[12];
    /* Make every sector permanent by using a dedicated callback context value. */
    fake.permanent_hole = 0;
    for (uint32_t lsn = 0; lsn < 12; ++lsn)
        fake.calls[lsn] = 0;
    media_recovery_result_t recovery = media_read_recover_limited(all_holes_read, &fake, 0, 12, 4,
                                                                   buffer, valid, 11);
    TEST_ASSERT_EQUAL_UINT32(11, recovery.holes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_short_read_recovers_remaining_sectors_and_retries_transient_error);
    RUN_TEST(test_permanent_hole_is_zero_filled_after_two_retries);
    RUN_TEST(test_fatal_input_is_not_retried);
    RUN_TEST(test_error_budget_allows_tenth_and_rejects_eleventh);
    RUN_TEST(test_limited_recovery_stops_at_eleventh_hole);
    return UNITY_END();
}
