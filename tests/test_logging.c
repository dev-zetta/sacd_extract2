#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unity.h"
#include "logging.h"

static char log_path[256];

static int fixed_time(struct timespec *now)
{
    now->tv_sec = 0;
    now->tv_nsec = 123000000;
    return 0;
}

void setUp(void)
{
    snprintf(log_path, sizeof(log_path), "/tmp/sacd-extract-log-test-%ld.log", (long)getpid());
    unlink(log_path);
}

void tearDown(void)
{
    destroy_logging();
    unlink(log_path);
}

static void test_buffered_records_flush_with_timestamp_severity_and_module(void)
{
    char content[2048];
    FILE *file;
    size_t length;
    init_logging(1, LOG_DEBUG, log_path);
    log_set_time_provider(fixed_time);
    LOG(lm_input, LOG_WARNING, ("read defect lsn=%u", 42u));
    TEST_ASSERT_EQUAL_INT(0, logging_open_session(NULL));
    destroy_logging();

    file = fopen(log_path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    content[length] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(content, "1970-01-01T"));
    TEST_ASSERT_NOT_NULL(strstr(content, ".123"));
    TEST_ASSERT_NOT_NULL(strstr(content, "WARNING"));
    TEST_ASSERT_NOT_NULL(strstr(content, "input"));
    TEST_ASSERT_NOT_NULL(strstr(content, "read defect lsn=42"));

    /* tearDown may safely call it again. */
    init_logging(0, LOG_INFO, NULL);
}

static void test_level_parser_accepts_documented_values(void)
{
    int valid = 0;
    TEST_ASSERT_EQUAL_INT(LOG_ERROR, logging_parse_level("error", &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_EQUAL_INT(LOG_DEBUG, logging_parse_level("DEBUG", &valid));
    TEST_ASSERT_TRUE(valid);
    (void)logging_parse_level("verbose", &valid);
    TEST_ASSERT_FALSE(valid);
}

static void test_generated_session_names_are_unique(void)
{
    char directory[256];
    char first[512];
    char second[512];
    snprintf(directory, sizeof(directory), "/tmp/sacd-extract-log-dir-%ld", (long)getpid());
    rmdir(directory);
    TEST_ASSERT_EQUAL_INT(0, mkdir(directory, 0700));

    init_logging(1, LOG_INFO, NULL);
    TEST_ASSERT_EQUAL_INT(0, logging_open_session(directory));
    TEST_ASSERT_NOT_NULL(logging_file_path());
    snprintf(first, sizeof(first), "%s", logging_file_path());
    destroy_logging();

    init_logging(1, LOG_INFO, NULL);
    TEST_ASSERT_EQUAL_INT(0, logging_open_session(directory));
    TEST_ASSERT_NOT_NULL(logging_file_path());
    snprintf(second, sizeof(second), "%s", logging_file_path());
    destroy_logging();

    TEST_ASSERT_NOT_EQUAL(0, strcmp(first, second));
    TEST_ASSERT_NOT_NULL(strstr(first, "sacd_extract-"));
    unlink(first);
    unlink(second);
    rmdir(directory);
    init_logging(0, LOG_INFO, NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_buffered_records_flush_with_timestamp_severity_and_module);
    RUN_TEST(test_level_parser_accepts_documented_values);
    RUN_TEST(test_generated_session_names_are_unique);
    return UNITY_END();
}
