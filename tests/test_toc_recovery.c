#include <string.h>

#include "unity.h"
#include "toc_recovery.h"

void setUp(void) {}
void tearDown(void) {}

static void test_area_headers_are_type_specific(void)
{
    const uint8_t stereo[] = "TWOCHTOCpayload";
    const uint8_t multi[] = "MULCHTOCpayload";
    TEST_ASSERT_TRUE(area_toc_header_valid(stereo, "TWOCHTOC"));
    TEST_ASSERT_TRUE(area_toc_header_valid(multi, "MULCHTOC"));
    TEST_ASSERT_FALSE(area_toc_header_valid(stereo, "MULCHTOC"));
    TEST_ASSERT_FALSE(area_toc_header_valid((const uint8_t *)"BROKEN!!", "TWOCHTOC"));
}

static void test_master_toc_is_mandatory(void)
{
    TEST_ASSERT_TRUE(master_toc_header_valid((const uint8_t *)"SACDMTOCpayload"));
    TEST_ASSERT_FALSE(master_toc_header_valid((const uint8_t *)"BROKEN!!payload"));
    TEST_ASSERT_FALSE(master_toc_header_valid(NULL));
}

static void test_primary_and_backup_validity_supports_area_omission(void)
{
    const uint8_t stereo[] = "TWOCHTOCprimary";
    const uint8_t stereo_backup[] = "TWOCHTOCbackup";
    const uint8_t broken[] = "BROKEN!!payload";

    TEST_ASSERT_TRUE(area_toc_header_valid(stereo, "TWOCHTOC"));
    TEST_ASSERT_TRUE(area_toc_header_valid(stereo_backup, "TWOCHTOC"));
    TEST_ASSERT_FALSE(area_toc_header_valid(broken, "TWOCHTOC"));
    TEST_ASSERT_FALSE(area_toc_header_valid(NULL, "TWOCHTOC"));
}

static void test_multichannel_backup_copy_uses_selected_buffer_and_area_size(void)
{
    uint8_t destination[16];
    uint8_t stereo_backup[16];
    uint8_t multichannel_backup[16];
    memset(destination, 0, sizeof(destination));
    memset(stereo_backup, 0x22, sizeof(stereo_backup));
    memset(multichannel_backup, 0x33, sizeof(multichannel_backup));

    area_toc_copy_backup(destination, multichannel_backup, 12);
    TEST_ASSERT_EACH_EQUAL_HEX8(0x33, destination, 12);
    TEST_ASSERT_EACH_EQUAL_HEX8(0x00, destination + 12, 4);
    TEST_ASSERT_FALSE(memcmp(destination, stereo_backup, 12) == 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_area_headers_are_type_specific);
    RUN_TEST(test_master_toc_is_mandatory);
    RUN_TEST(test_primary_and_backup_validity_supports_area_omission);
    RUN_TEST(test_multichannel_backup_copy_uses_selected_buffer_and_area_size);
    return UNITY_END();
}
