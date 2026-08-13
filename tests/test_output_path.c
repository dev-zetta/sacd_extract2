#include <stdio.h>

#include "unity.h"
#include "output_path.h"

static char renamed_from[128];
static char renamed_to[128];

static int fake_rename(const char *source, const char *destination)
{
    snprintf(renamed_from, sizeof(renamed_from), "%s", source);
    snprintf(renamed_to, sizeof(renamed_to), "%s", destination);
    return 0;
}

void setUp(void)
{
    renamed_from[0] = '\0';
    renamed_to[0] = '\0';
}
void tearDown(void) { output_set_rename_hook(NULL); }

static void test_marker_precedes_media_extension(void)
{
    char path[128];
    TEST_ASSERT_EQUAL_INT(0, output_path_with_marker("Album/Track.dsf", "partial", path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("Album/Track.partial.dsf", path);
    TEST_ASSERT_EQUAL_INT(0, output_path_with_marker("Disc.iso", "inprogress", path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("Disc.inprogress.iso", path);
}

static void test_marker_handles_extensionless_and_dotted_directories(void)
{
    char path[128];
    TEST_ASSERT_EQUAL_INT(0, output_path_with_marker("a.b/Track", "failed", path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("a.b/Track.failed", path);
}

static void test_marker_rejects_truncation(void)
{
    char path[8];
    TEST_ASSERT_EQUAL_INT(-1, output_path_with_marker("Track.dsf", "partial", path, sizeof(path)));
}

static void test_atomic_publish_uses_injected_output_hook(void)
{
    output_set_rename_hook(fake_rename);
    TEST_ASSERT_EQUAL_INT(0, output_publish_atomic("Track.inprogress.dsf", "Track.partial.dsf"));
    TEST_ASSERT_EQUAL_STRING("Track.inprogress.dsf", renamed_from);
    TEST_ASSERT_EQUAL_STRING("Track.partial.dsf", renamed_to);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_marker_precedes_media_extension);
    RUN_TEST(test_marker_handles_extensionless_and_dotted_directories);
    RUN_TEST(test_marker_rejects_truncation);
    RUN_TEST(test_atomic_publish_uses_injected_output_hook);
    return UNITY_END();
}
