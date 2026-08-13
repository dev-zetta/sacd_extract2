/*
 * Direct SACD DSD64 to PCM FLAC output.
 *
 * Copyright (c) 2026 by respective authors.
 * GPL-2.0-or-later; libFLAC is used under the Xiph BSD license.
 */

#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WIN32) || defined(_WIN32)
#include <io.h>
#else
#include <sys/types.h>
#endif

#include "dsd_pcm.h"
#include "scarletbook_output.h"

typedef struct flac_output_handle_s
{
    FLAC__StreamEncoder *encoder;
    FLAC__StreamMetadata *metadata[2];
    unsigned int metadata_count;
    int encoder_initialized;
    dsd_pcm_state_t converter;
    FLAC__int32 *pcm;
    size_t pcm_capacity_frames;
} flac_output_handle_t;

static FLAC__StreamEncoderWriteStatus flac_write_callback(
    const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[],
    size_t bytes, unsigned int samples, unsigned int current_frame,
    void *client_data)
{
    scarletbook_output_format_t *ft = client_data;
    (void)encoder;
    (void)samples;
    (void)current_frame;
    return ft && ft->fd && fwrite(buffer, 1, bytes, ft->fd) == bytes
               ? FLAC__STREAM_ENCODER_WRITE_STATUS_OK
               : FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}

static FLAC__StreamEncoderSeekStatus flac_seek_callback(
    const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset,
    void *client_data)
{
    scarletbook_output_format_t *ft = client_data;
    (void)encoder;
#if defined(WIN32) || defined(_WIN32)
    return ft && ft->fd && _fseeki64(ft->fd, (__int64)absolute_byte_offset, SEEK_SET) == 0
#else
    return ft && ft->fd && fseeko(ft->fd, (off_t)absolute_byte_offset, SEEK_SET) == 0
#endif
               ? FLAC__STREAM_ENCODER_SEEK_STATUS_OK
               : FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
}

static FLAC__StreamEncoderTellStatus flac_tell_callback(
    const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset,
    void *client_data)
{
    scarletbook_output_format_t *ft = client_data;
    (void)encoder;
    if (!ft || !ft->fd || !absolute_byte_offset)
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
#if defined(WIN32) || defined(_WIN32)
    __int64 offset = _ftelli64(ft->fd);
#else
    off_t offset = ftello(ft->fd);
#endif
    if (offset < 0)
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = (FLAC__uint64)offset;
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static const char *first_text(const char *preferred, const char *phonetic)
{
    return preferred ? preferred : phonetic;
}

static int add_comment(FLAC__StreamMetadata *comments, const char *name,
                       const char *value)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    if (!comments || !name || !value || !*value)
        return 1;
    if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
            &entry, name, value))
        return 0;
    if (!FLAC__metadata_object_vorbiscomment_append_comment(comments, entry,
                                                             false))
    {
        free(entry.entry);
        return 0;
    }
    return 1;
}

static int add_number_comment(FLAC__StreamMetadata *comments, const char *name,
                              unsigned int value)
{
    char text[32];
    snprintf(text, sizeof(text), "%u", value);
    return add_comment(comments, name, text);
}

static int create_metadata(scarletbook_output_format_t *ft,
                           flac_output_handle_t *flac)
{
    scarletbook_handle_t *handle = ft->sb_handle;
    int valid_area = handle && ft->area >= 0 && ft->area < handle->area_count &&
                     handle->area[ft->area].area_toc;
    int valid_track = valid_area && ft->track >= 0 &&
                      ft->track < handle->area[ft->area].area_toc->track_count;
    FLAC__StreamMetadata *comments =
        FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__StreamMetadata *padding =
        FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
    const char *title = NULL;
    const char *album = NULL;
    const char *artist = NULL;
    const char *album_artist = NULL;
    char date[16];

    if (!comments || !padding)
    {
        FLAC__metadata_object_delete(comments);
        FLAC__metadata_object_delete(padding);
        return -1;
    }
    padding->length = 2048;

    if (valid_track)
    {
        area_track_text_t *track_text =
            &handle->area[ft->area].area_track_text[ft->track];
        title = first_text(track_text->track_type_title,
                           track_text->track_type_title_phonetic);
        artist = first_text(track_text->track_type_performer,
                            track_text->track_type_performer_phonetic);
    }
    if (handle)
    {
        album = first_text(handle->master_text.album_title,
                           handle->master_text.album_title_phonetic);
        if (!album)
            album = first_text(handle->master_text.disc_title,
                               handle->master_text.disc_title_phonetic);
        album_artist = first_text(handle->master_text.album_artist,
                                  handle->master_text.album_artist_phonetic);
        if (!artist)
            artist = first_text(handle->master_text.disc_artist,
                                handle->master_text.disc_artist_phonetic);
        if (!artist)
            artist = album_artist;
    }

    if (!add_comment(comments, "TITLE", title) ||
        !add_comment(comments, "ALBUM", album) ||
        !add_comment(comments, "ARTIST", artist) ||
        !add_comment(comments, "ALBUMARTIST", album_artist) ||
        !add_number_comment(comments, "TRACKNUMBER", (unsigned int)ft->track + 1u) ||
        !add_comment(comments, "SOURCEMEDIA", "SACD") ||
        !add_comment(comments, "COMMENT",
                     "DSD64 converted to 24-bit PCM by sacd_extract2"))
        goto error;

    if (valid_area)
    {
        if (!add_number_comment(comments, "TRACKTOTAL",
                handle->area[ft->area].area_toc->track_count))
            goto error;
    }
    if (handle && handle->master_toc)
    {
        if (!add_number_comment(comments, "DISCNUMBER",
                                handle->master_toc->album_sequence_number) ||
            !add_number_comment(comments, "DISCTOTAL",
                                handle->master_toc->album_set_size))
            goto error;
        if (handle->master_toc->disc_date_year)
        {
            snprintf(date, sizeof(date), "%04u-%02u-%02u",
                     handle->master_toc->disc_date_year,
                     handle->master_toc->disc_date_month,
                     handle->master_toc->disc_date_day);
            if (!add_comment(comments, "DATE", date))
                goto error;
        }
    }

    flac->metadata[0] = comments;
    flac->metadata[1] = padding;
    flac->metadata_count = 2;
    return 0;

error:
    FLAC__metadata_object_delete(comments);
    FLAC__metadata_object_delete(padding);
    return -1;
}

static int flac_create(scarletbook_output_format_t *ft)
{
    flac_output_handle_t *flac = (flac_output_handle_t *)ft->priv;
    FLAC__StreamEncoderInitStatus init_status;
    FLAC__uint64 total_samples = 0;

    if (!flac || ft->channel_count < 1 || ft->channel_count > 8 ||
        dsd_pcm_init(&flac->converter, (unsigned int)ft->channel_count,
                     ft->flac_sample_rate, 4.0, 1) != 0)
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE,
                                            "unsupported FLAC conversion settings");
        return -1;
    }

    flac->encoder = FLAC__stream_encoder_new();
    if (!flac->encoder || create_metadata(ft, flac) != 0)
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE,
                                            "unable to allocate FLAC encoder");
        return -1;
    }

    if (ft->sb_handle && ft->area >= 0 &&
        ft->area < ft->sb_handle->area_count &&
        ft->sb_handle->area[ft->area].area_toc &&
        ft->sb_handle->area[ft->area].area_tracklist_time &&
        ft->track >= 0 &&
        ft->track < ft->sb_handle->area[ft->area].area_toc->track_count)
    {
        uint32_t frames = TIME_FRAMECOUNT(
            &ft->sb_handle->area[ft->area].area_tracklist_time->duration[ft->track]);
        total_samples = (FLAC__uint64)frames * ft->flac_sample_rate / SACD_FRAME_RATE;
    }

    if (!FLAC__stream_encoder_set_verify(flac->encoder, true) ||
        !FLAC__stream_encoder_set_compression_level(flac->encoder, 5) ||
        !FLAC__stream_encoder_set_channels(flac->encoder,
                                           (unsigned int)ft->channel_count) ||
        !FLAC__stream_encoder_set_bits_per_sample(flac->encoder, 24) ||
        !FLAC__stream_encoder_set_sample_rate(flac->encoder,
                                              ft->flac_sample_rate) ||
        (total_samples && !FLAC__stream_encoder_set_total_samples_estimate(
                              flac->encoder, total_samples)) ||
        !FLAC__stream_encoder_set_metadata(flac->encoder, flac->metadata,
                                           flac->metadata_count))
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE,
                                            "unable to configure FLAC encoder");
        return -1;
    }

    init_status = FLAC__stream_encoder_init_stream(
        flac->encoder, flac_write_callback, flac_seek_callback,
        flac_tell_callback, NULL, ft);
    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
    {
        scarletbook_output_format_set_error(
            ft, SACD_OUTPUT_ERROR_CREATE,
            FLAC__StreamEncoderInitStatusString[init_status]);
        return -1;
    }
    flac->encoder_initialized = 1;
    return 0;
}

static int ensure_pcm_capacity(flac_output_handle_t *flac, size_t frames,
                               unsigned int channels)
{
    FLAC__int32 *resized;
    if (frames <= flac->pcm_capacity_frames)
        return 0;
    resized = realloc(flac->pcm, frames * channels * sizeof(*resized));
    if (!resized)
        return -1;
    flac->pcm = resized;
    flac->pcm_capacity_frames = frames;
    return 0;
}

static int flac_write_frame(scarletbook_output_format_t *ft,
                            const uint8_t *buf, size_t len)
{
    flac_output_handle_t *flac = (flac_output_handle_t *)ft->priv;
    size_t capacity = dsd_pcm_max_output_frames(&flac->converter, len);
    size_t frames = 0;

    if (!flac || !flac->encoder_initialized ||
        ensure_pcm_capacity(flac, capacity, (unsigned int)ft->channel_count) != 0 ||
        dsd_pcm_convert(&flac->converter, buf, len, flac->pcm, capacity,
                        &frames) != 0)
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_WRITE,
                                            "DSD to PCM conversion failed");
        return -1;
    }
    if (frames && !FLAC__stream_encoder_process_interleaved(flac->encoder,
                                                             flac->pcm,
                                                             (unsigned int)frames))
    {
        scarletbook_output_format_set_error(
            ft, SACD_OUTPUT_ERROR_WRITE,
            FLAC__stream_encoder_get_resolved_state_string(flac->encoder));
        return -1;
    }
    return (int)len;
}

static void flac_discontinuity(scarletbook_output_format_t *ft)
{
    flac_output_handle_t *flac = (flac_output_handle_t *)ft->priv;
    if (flac)
        dsd_pcm_reset(&flac->converter);
}

static int flac_close(scarletbook_output_format_t *ft)
{
    flac_output_handle_t *flac = (flac_output_handle_t *)ft->priv;
    int result = 0;
    if (!flac)
        return -1;
    if (flac->encoder_initialized &&
        !FLAC__stream_encoder_finish(flac->encoder))
        result = -1;
    flac->encoder_initialized = 0;
    FLAC__stream_encoder_delete(flac->encoder);
    flac->encoder = NULL;
    for (unsigned int i = 0; i < flac->metadata_count; ++i)
        FLAC__metadata_object_delete(flac->metadata[i]);
    flac->metadata_count = 0;
    free(flac->pcm);
    flac->pcm = NULL;
    flac->pcm_capacity_frames = 0;
    dsd_pcm_destroy(&flac->converter);
    return result;
}

scarletbook_format_handler_t const *flac_format_fn(void)
{
    static scarletbook_format_handler_t handler = {
        "24-bit PCM Free Lossless Audio Codec",
        "flac",
        flac_create,
        flac_write_frame,
        flac_close,
        flac_discontinuity,
        OUTPUT_FLAG_DSD,
        sizeof(flac_output_handle_t)
    };
    return &handler;
}
