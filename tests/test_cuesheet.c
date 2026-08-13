#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "unity.h"
#include "cuesheet.h"

void setUp(void) {}
void tearDown(void) {}

static void test_synthetic_metadata_produces_deterministic_cue(void)
{
    scarletbook_handle_t handle;
    master_toc_t master;
    area_toc_t area_toc;
    area_tracklist_t times;
    area_isrc_genre_t isrc;
    char path[256];
    char content[4096];
    FILE *file;
    size_t length;

    memset(&handle, 0, sizeof(handle));
    memset(&master, 0, sizeof(master));
    memset(&area_toc, 0, sizeof(area_toc));
    memset(&times, 0, sizeof(times));
    memset(&isrc, 0, sizeof(isrc));
    handle.master_toc = &master;
    handle.master_text.disc_artist = "Test Artist";
    handle.master_text.disc_title = "Test Album";
    master.disc_date_year = 2026;
    master.disc_date_month = 8;
    master.disc_date_day = 13;
    area_toc.track_count = 2;
    area_toc.channel_count = 2;
    handle.area[0].area_toc = &area_toc;
    handle.area[0].area_tracklist_time = &times;
    handle.area[0].area_isrc_genre = &isrc;
    handle.area[0].area_track_text[0].track_type_title = "First";
    handle.area[0].area_track_text[1].track_type_title = "Second";
    times.start[0].frames = 2;
    times.duration[0].minutes = 3;
    times.start[1].minutes = 3;
    times.start[1].seconds = 1;

    snprintf(path, sizeof(path), "sacd-cue-test-%ld.cue", (long)getpid());
    unlink(path);
    TEST_ASSERT_EQUAL_INT(0, write_cue_sheet(&handle, "Test Album.dff", 0, path));
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    unlink(path);
    content[length] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(content, "REM DATE 2026-08-13"));
    TEST_ASSERT_NOT_NULL(strstr(content, "PERFORMER \"Test Artist\""));
    TEST_ASSERT_NOT_NULL(strstr(content, "TITLE \"Test Album\""));
    TEST_ASSERT_NOT_NULL(strstr(content, "FILE \"Test Album.dff\" WAVE"));
    TEST_ASSERT_NOT_NULL(strstr(content, "TRACK 02 AUDIO"));
}

static void test_track_file_cue_references_actual_dsf_files(void)
{
    scarletbook_handle_t handle;
    master_toc_t master;
    area_toc_t area_toc;
    area_tracklist_t times;
    area_isrc_genre_t isrc;
    char path[256];
    char content[4096];
    FILE *file;
    size_t length;
    uint8_t selected_tracks[256] = {0};

    memset(&handle, 0, sizeof(handle));
    memset(&master, 0, sizeof(master));
    memset(&area_toc, 0, sizeof(area_toc));
    memset(&times, 0, sizeof(times));
    memset(&isrc, 0, sizeof(isrc));
    handle.master_toc = &master;
    handle.master_text.disc_artist = "Test Artist";
    handle.master_text.disc_title = "Test Album";
    handle.audio_frame_trimming = 0;
    area_toc.track_count = 2;
    area_toc.channel_count = 2;
    handle.area[0].area_toc = &area_toc;
    handle.area[0].area_tracklist_time = &times;
    handle.area[0].area_isrc_genre = &isrc;
    handle.area[0].area_track_text[0].track_type_title = "First";
    handle.area[0].area_track_text[1].track_type_title = "Second";
    times.start[0].seconds = 2;
    times.start[1].minutes = 3;

    snprintf(path, sizeof(path), "sacd-track-cue-test-%ld.cue", (long)getpid());
    unlink(path);
    TEST_ASSERT_EQUAL_INT(0, write_track_cue_sheet(&handle, "dsf", 0, path, NULL));
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    unlink(path);
    content[length] = '\0';

    TEST_ASSERT_NOT_NULL(strstr(content, "FILE \"01 - First.dsf\" WAVE\n  TRACK 01 AUDIO"));
    TEST_ASSERT_NOT_NULL(strstr(content, "INDEX 00 00:00:00\n      INDEX 01 00:02:00"));
    TEST_ASSERT_NOT_NULL(strstr(content, "FILE \"02 - Second.dsf\" WAVE\n  TRACK 02 AUDIO"));
    TEST_ASSERT_NOT_NULL(strstr(content, "TRACK 02 AUDIO\n      TITLE \"Second\"\n      INDEX 01 00:00:00"));
    TEST_ASSERT_NULL(strstr(content, "Test Album.dff"));

    selected_tracks[1] = 1;
    TEST_ASSERT_EQUAL_INT(0, write_track_cue_sheet(&handle, "dsf", 0, path, selected_tracks));
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    unlink(path);
    content[length] = '\0';
    TEST_ASSERT_NULL(strstr(content, "01 - First.dsf"));
    TEST_ASSERT_NOT_NULL(strstr(content, "FILE \"02 - Second.dsf\" WAVE\n  TRACK 01 AUDIO"));
    TEST_ASSERT_NULL(strstr(content, "TRACK 02 AUDIO"));

    TEST_ASSERT_EQUAL_INT(0, write_track_cue_sheet(&handle, "flac", 0, path, NULL));
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    unlink(path);
    content[length] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(content, "FILE \"01 - First.flac\" WAVE"));
    TEST_ASSERT_NOT_NULL(strstr(content, "FILE \"02 - Second.flac\" WAVE"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_synthetic_metadata_produces_deterministic_cue);
    RUN_TEST(test_track_file_cue_references_actual_dsf_files);
    return UNITY_END();
}
