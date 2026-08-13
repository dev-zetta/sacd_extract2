#include <string.h>

#include "unity.h"
#include "scarletbook_output.h"
#include "output_status.h"

void setUp(void) {}
void tearDown(void) {}

static void test_exit_statuses_preserve_highest_severity(void)
{
    TEST_ASSERT_EQUAL_INT(0, sacd_output_merge_exit_status(0, SACD_OUTPUT_RESULT_CLEAN));
    TEST_ASSERT_EQUAL_INT(1, sacd_output_merge_exit_status(0, SACD_OUTPUT_RESULT_PARTIAL));
    TEST_ASSERT_EQUAL_INT(2, sacd_output_merge_exit_status(1, SACD_OUTPUT_RESULT_FATAL));
    TEST_ASSERT_EQUAL_INT(130, sacd_output_merge_exit_status(2, SACD_OUTPUT_RESULT_INTERRUPTED));
    TEST_ASSERT_EQUAL_INT(2, sacd_output_merge_exit_status(2, SACD_OUTPUT_RESULT_CLEAN));
}

static void test_partial_track_continues_queue_but_fatal_and_interrupt_stop(void)
{
    TEST_ASSERT_TRUE(sacd_output_queue_continues(SACD_OUTPUT_RESULT_CLEAN));
    TEST_ASSERT_TRUE(sacd_output_queue_continues(SACD_OUTPUT_RESULT_PARTIAL));
    TEST_ASSERT_FALSE(sacd_output_queue_continues(SACD_OUTPUT_RESULT_FATAL));
    TEST_ASSERT_FALSE(sacd_output_queue_continues(SACD_OUTPUT_RESULT_INTERRUPTED));
}

static void test_error_number_and_string_propagate_for_output_failures(void)
{
    scarletbook_output_format_t format;
    memset(&format, 0, sizeof(format));

    scarletbook_output_format_set_error(&format, SACD_OUTPUT_ERROR_CREATE, "create failed");
    TEST_ASSERT_EQUAL_INT(SACD_OUTPUT_ERROR_CREATE, format.error_number);
    TEST_ASSERT_EQUAL_STRING("create failed", format.error_str);
    scarletbook_output_format_set_error(&format, SACD_OUTPUT_ERROR_WRITE, "write failed");
    TEST_ASSERT_EQUAL_INT(SACD_OUTPUT_ERROR_WRITE, format.error_number);
    scarletbook_output_format_set_error(&format, SACD_OUTPUT_ERROR_FINALIZE, "finalize failed");
    TEST_ASSERT_EQUAL_INT(SACD_OUTPUT_ERROR_FINALIZE, format.error_number);
    scarletbook_output_format_set_error(&format, SACD_OUTPUT_ERROR_MEDIA, "media failed");
    TEST_ASSERT_EQUAL_INT(SACD_OUTPUT_ERROR_MEDIA, format.error_number);
    TEST_ASSERT_EQUAL_STRING("media failed", format.error_str);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exit_statuses_preserve_highest_severity);
    RUN_TEST(test_partial_track_continues_queue_but_fatal_and_interrupt_stop);
    RUN_TEST(test_error_number_and_string_propagate_for_output_failures);
    return UNITY_END();
}
