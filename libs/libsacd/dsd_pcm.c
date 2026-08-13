/*
 * Streaming DSD64 to PCM conversion.
 *
 * Filter coefficients and the byte lookup technique are adapted from
 * dsf2flac (https://github.com/hank/dsf2flac), GPL-2.0-or-later.
 */

#include "dsd_pcm.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dsd_pcm_filters.inc"

#define DSD64_SAMPLE_RATE 2822400u
#define PCM_BITS 24u
#define PCM_MAX 8388607.0

static uint32_t dsd_pcm_random(dsd_pcm_state_t *state)
{
    uint32_t value = state->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state->random_state = value;
    return value;
}

static double dsd_pcm_uniform(dsd_pcm_state_t *state)
{
    return (double)dsd_pcm_random(state) / (double)UINT32_MAX;
}

static int select_filter(unsigned int sample_rate, const double **coefficients,
                         int *coefficient_count)
{
    switch (sample_rate)
    {
    case 88200:
        *coefficients = coefs_88;
        *coefficient_count = nCoefs_88;
        return 0;
    case 176400:
        *coefficients = coefs_176;
        *coefficient_count = nCoefs_176;
        return 0;
    case 352800:
        *coefficients = coefs_352;
        *coefficient_count = nCoefs_352;
        return 0;
    default:
        return -1;
    }
}

int dsd_pcm_init(dsd_pcm_state_t *state, unsigned int channels,
                 unsigned int sample_rate, double scale_db, int dither)
{
    const double *coefficients = NULL;
    int coefficient_count = 0;

    if (!state || channels == 0 || channels > 8 ||
        select_filter(sample_rate, &coefficients, &coefficient_count) != 0 ||
        DSD64_SAMPLE_RATE % sample_rate != 0 ||
        (DSD64_SAMPLE_RATE / sample_rate) % 8 != 0)
        return -1;

    memset(state, 0, sizeof(*state));
    state->channels = channels;
    state->sample_rate = sample_rate;
    state->step_bytes = (DSD64_SAMPLE_RATE / sample_rate) / 8;
    state->lookup_rows = (unsigned int)(coefficient_count + 7) / 8u;
    state->history_rows = state->lookup_rows;
    state->scale = pow(10.0, scale_db / 20.0) * PCM_MAX;
    state->dither = dither != 0;
    state->random_state = 0x6d2b79f5u;
    state->lookup = calloc((size_t)state->lookup_rows * 256u, sizeof(double));
    state->history = calloc((size_t)channels * state->history_rows, 1);
    if (!state->lookup || !state->history)
    {
        dsd_pcm_destroy(state);
        return -1;
    }

    for (unsigned int row = 0; row < state->lookup_rows; ++row)
    {
        int bits = coefficient_count - (int)(row * 8u);
        if (bits > 8)
            bits = 8;
        for (unsigned int sequence = 0; sequence < 256u; ++sequence)
        {
            double sum = 0.0;
            for (int bit = 0; bit < bits; ++bit)
            {
                double dsd = (sequence & (1u << (7 - bit))) ? 1.0 : -1.0;
                sum += dsd * coefficients[row * 8u + (unsigned int)bit];
            }
            state->lookup[row * 256u + sequence] = sum;
        }
    }
    dsd_pcm_reset(state);
    return 0;
}

void dsd_pcm_reset(dsd_pcm_state_t *state)
{
    if (!state)
        return;
    state->history_filled = 0;
    state->history_position = 0;
    state->groups_until_output = 0;
    if (state->history)
        memset(state->history, 0,
               (size_t)state->channels * state->history_rows);
}

void dsd_pcm_destroy(dsd_pcm_state_t *state)
{
    if (!state)
        return;
    free(state->lookup);
    free(state->history);
    memset(state, 0, sizeof(*state));
}

size_t dsd_pcm_max_output_frames(const dsd_pcm_state_t *state,
                                 size_t input_bytes)
{
    size_t groups;
    if (!state || state->channels == 0 || state->step_bytes == 0)
        return 0;
    groups = input_bytes / state->channels;
    return groups / state->step_bytes + 1u;
}

static int32_t convert_sample(dsd_pcm_state_t *state, unsigned int channel)
{
    double sum = 0.0;
    for (unsigned int row = 0; row < state->lookup_rows; ++row)
    {
        unsigned int position = (state->history_position + row) % state->history_rows;
        uint8_t sequence = state->history[channel * state->history_rows + position];
        sum += state->lookup[row * 256u + sequence];
    }
    sum *= state->scale;
    if (state->dither)
        sum += dsd_pcm_uniform(state) - dsd_pcm_uniform(state);
    if (sum > PCM_MAX)
        sum = PCM_MAX;
    else if (sum < -PCM_MAX)
        sum = -PCM_MAX;
    return (int32_t)llround(sum);
}

int dsd_pcm_convert(dsd_pcm_state_t *state, const uint8_t *input,
                    size_t input_bytes, int32_t *output,
                    size_t output_capacity_frames,
                    size_t *output_frames)
{
    size_t frames = 0;
    size_t groups;

    if (output_frames)
        *output_frames = 0;
    if (!state || !input || !output || !output_frames || state->channels == 0 ||
        input_bytes % state->channels != 0)
        return -1;

    groups = input_bytes / state->channels;
    for (size_t group = 0; group < groups; ++group)
    {
        for (unsigned int channel = 0; channel < state->channels; ++channel)
        {
            state->history[channel * state->history_rows + state->history_position] =
                input[group * state->channels + channel];
        }
        state->history_position =
            (state->history_position + 1u) % state->history_rows;

        if (state->history_filled < state->history_rows)
        {
            state->history_filled++;
            if (state->history_filled < state->history_rows)
                continue;
        }

        if (state->groups_until_output > 0)
        {
            state->groups_until_output--;
            continue;
        }
        if (frames >= output_capacity_frames)
            return -1;
        for (unsigned int channel = 0; channel < state->channels; ++channel)
            output[frames * state->channels + channel] = convert_sample(state, channel);
        frames++;
        state->groups_until_output = state->step_bytes - 1u;
    }

    *output_frames = frames;
    return 0;
}
