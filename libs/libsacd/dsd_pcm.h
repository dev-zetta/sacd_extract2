/*
 * Streaming DSD64 to PCM conversion.
 *
 * The FIR coefficient sets are adapted from dsf2flac, GPL-2.0-or-later.
 */
#ifndef SACD_DSD_PCM_H_INCLUDED
#define SACD_DSD_PCM_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

typedef struct dsd_pcm_state_s
{
    unsigned int channels;
    unsigned int sample_rate;
    unsigned int step_bytes;
    unsigned int lookup_rows;
    unsigned int history_rows;
    unsigned int history_filled;
    unsigned int history_position;
    unsigned int groups_until_output;
    double scale;
    int dither;
    uint32_t random_state;
    double *lookup;
    uint8_t *history;
} dsd_pcm_state_t;

int dsd_pcm_init(dsd_pcm_state_t *state, unsigned int channels,
                 unsigned int sample_rate, double scale_db, int dither);
void dsd_pcm_reset(dsd_pcm_state_t *state);
void dsd_pcm_destroy(dsd_pcm_state_t *state);
size_t dsd_pcm_max_output_frames(const dsd_pcm_state_t *state,
                                 size_t input_bytes);
int dsd_pcm_convert(dsd_pcm_state_t *state, const uint8_t *input,
                    size_t input_bytes, int32_t *output,
                    size_t output_capacity_frames,
                    size_t *output_frames);

#endif
