#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dsd_pcm.h"

void setUp(void) {}
void tearDown(void) {}

static size_t convert_all(dsd_pcm_state_t *state, const uint8_t *input,
                          size_t input_size, int32_t *output, size_t capacity)
{
    size_t frames = 0;
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_convert(state, input, input_size, output,
                                             capacity, &frames));
    return frames;
}

static void test_constant_dsd_has_expected_polarity_and_channels(void)
{
    dsd_pcm_state_t positive;
    dsd_pcm_state_t negative;
    uint8_t ones[2048];
    uint8_t zeroes[2048];
    int32_t positive_pcm[1024];
    int32_t negative_pcm[1024];
    size_t positive_frames;
    size_t negative_frames;

    memset(ones, 0xff, sizeof(ones));
    memset(zeroes, 0x00, sizeof(zeroes));
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_init(&positive, 2, 88200, 4.0, 0));
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_init(&negative, 2, 88200, 4.0, 0));
    positive_frames = convert_all(&positive, ones, sizeof(ones), positive_pcm, 512);
    negative_frames = convert_all(&negative, zeroes, sizeof(zeroes), negative_pcm, 512);

    TEST_ASSERT_GREATER_THAN(0, positive_frames);
    TEST_ASSERT_EQUAL_UINT(positive_frames, negative_frames);
    TEST_ASSERT_GREATER_THAN(0, positive_pcm[0]);
    TEST_ASSERT_LESS_THAN(0, negative_pcm[0]);
    for (size_t frame = 0; frame < positive_frames; ++frame)
    {
        TEST_ASSERT_EQUAL_INT32(positive_pcm[frame * 2], positive_pcm[frame * 2 + 1]);
        TEST_ASSERT_EQUAL_INT32(negative_pcm[frame * 2], negative_pcm[frame * 2 + 1]);
    }

    dsd_pcm_destroy(&positive);
    dsd_pcm_destroy(&negative);
}

static void test_streaming_chunks_match_one_shot_conversion(void)
{
    dsd_pcm_state_t one_shot;
    dsd_pcm_state_t chunked;
    uint8_t input[2000];
    int32_t expected[4000];
    int32_t actual[4000];
    size_t expected_frames;
    size_t first_frames = 0;
    size_t second_frames = 0;

    for (size_t index = 0; index < sizeof(input); ++index)
        input[index] = (uint8_t)(index * 37u + 11u);
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_init(&one_shot, 2, 176400, 4.0, 1));
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_init(&chunked, 2, 176400, 4.0, 1));

    expected_frames = convert_all(&one_shot, input, sizeof(input), expected, 2000);
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_convert(&chunked, input, 246, actual,
                                             2000, &first_frames));
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_convert(&chunked, input + 246,
                                             sizeof(input) - 246,
                                             actual + first_frames * 2,
                                             2000 - first_frames,
                                             &second_frames));

    TEST_ASSERT_EQUAL_UINT(expected_frames, first_frames + second_frames);
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, actual, expected_frames * 2);
    dsd_pcm_destroy(&one_shot);
    dsd_pcm_destroy(&chunked);
}

static void test_reset_discards_filter_history_at_a_media_hole(void)
{
    dsd_pcm_state_t state;
    uint8_t ones[1024];
    uint8_t zeroes[1024];
    int32_t pcm[1024];
    size_t frames;

    memset(ones, 0xff, sizeof(ones));
    memset(zeroes, 0x00, sizeof(zeroes));
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_init(&state, 2, 352800, 4.0, 0));
    frames = convert_all(&state, ones, sizeof(ones), pcm, 512);
    TEST_ASSERT_GREATER_THAN(0, frames);
    TEST_ASSERT_GREATER_THAN(0, pcm[(frames - 1) * 2]);

    dsd_pcm_reset(&state);
    frames = convert_all(&state, zeroes, sizeof(zeroes), pcm, 512);
    TEST_ASSERT_GREATER_THAN(0, frames);
    TEST_ASSERT_LESS_THAN(0, pcm[0]);
    dsd_pcm_destroy(&state);
}

static void test_rejects_unsupported_rates_and_partial_channel_groups(void)
{
    dsd_pcm_state_t state;
    uint8_t input[3] = {0, 0, 0};
    int32_t output[4];
    size_t frames = 99;

    TEST_ASSERT_EQUAL_INT(-1, dsd_pcm_init(&state, 2, 96000, 4.0, 1));
    TEST_ASSERT_EQUAL_INT(0, dsd_pcm_init(&state, 2, 88200, 4.0, 1));
    TEST_ASSERT_EQUAL_INT(-1, dsd_pcm_convert(&state, input, sizeof(input),
                                              output, 2, &frames));
    TEST_ASSERT_EQUAL_UINT(0, frames);
    dsd_pcm_destroy(&state);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_constant_dsd_has_expected_polarity_and_channels);
    RUN_TEST(test_streaming_chunks_match_one_shot_conversion);
    RUN_TEST(test_reset_discards_filter_history_at_a_media_hole);
    RUN_TEST(test_rejects_unsupported_rates_and_partial_channel_groups);
    return UNITY_END();
}
