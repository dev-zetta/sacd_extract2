/**
 * SACD Ripper - https://github.com/sacd-ripper/
 *
 * Copyright (c) 2010-2015 by respective authors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#ifndef __APPLE__
#include <malloc.h>
#endif
#include <errno.h>
#include <assert.h>
#if defined(WIN32) || defined(_WIN32)
#include <io.h>
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>

#include <charset.h>
#include <utils.h>
#include <logging.h>
#include <fileutils.h>

#include "scarletbook_output.h"
#include "scarletbook_read.h"
#include "sacd_reader.h"
#include "media_recovery.h"
#include "output_path.h"
#include "output_status.h"


extern scarletbook_format_handler_t const * dsdiff_format_fn(void);
extern scarletbook_format_handler_t const * dsdiff_edit_master_format_fn(void);
extern scarletbook_format_handler_t const * dsf_format_fn(void);
extern scarletbook_format_handler_t const * flac_format_fn(void);
extern scarletbook_format_handler_t const * iso_format_fn(void);

typedef const scarletbook_format_handler_t *(*sacd_output_format_fn_t)(void);
static sacd_output_format_fn_t s_sacd_output_format_fns[] =
{
    dsdiff_format_fn,
    dsdiff_edit_master_format_fn,
    dsf_format_fn,
    flac_format_fn,
    iso_format_fn,
    NULL
};

void scarletbook_output_format_set_error(scarletbook_output_format_t *format,
                                         scarletbook_output_error_t error_number,
                                         const char *error_string)
{
    if (!format)
        return;
    format->error_number = error_number;
    snprintf(format->error_str, sizeof(format->error_str), "%s",
             error_string ? error_string : "output error");
}

struct scarletbook_output_s
{
    struct list_head    ripping_queue;

    uint8_t            *read_buffer;

    pthread_t           processing_thread_id;
    atomic_int          stop_processing;            // indicates if the thread needs to stop or has stopped
    atomic_int          processing;

    // stats
    int                 stats_total_tracks;
    int                 stats_current_track;
    uint32_t            stats_total_sectors;
    uint32_t            stats_total_sectors_processed;
    uint32_t            stats_current_file_total_sectors;
    uint32_t            stats_current_file_sectors_processed;
    stats_progress_callback_t stats_progress_callback;
    stats_track_callback_t stats_track_callback;

    fwprintf_callback_t fwprintf_callback;

    scarletbook_handle_t *sb_handle;
    uint32_t max_media_errors;
    unsigned int flac_sample_rate;
    int result;
    int interrupted;
};

static void init_output_format(scarletbook_output_t *output, scarletbook_output_format_t *ft)
{
    ft->max_media_errors = output->max_media_errors;
    ft->flac_sample_rate = output->flac_sample_rate;
    atomic_init(&ft->media_error_count, 0);
    atomic_init(&ft->abort_current, 0);
}

static scarletbook_format_handler_t const * find_output_format(char const * name)
{
    int i=0;
    while (s_sacd_output_format_fns[i] != NULL)
    {
        scarletbook_format_handler_t const * handler = s_sacd_output_format_fns[i]();
        if (!strcasecmp(handler->name, name))
        {
            return handler;
        }
        i++;
    }
    return NULL;
}

static void destroy_ripping_queue(scarletbook_output_t *output)
{
    struct list_head * node_ptr;
    scarletbook_output_format_t * output_format_ptr;

    while (!list_empty(&output->ripping_queue))
    {
        node_ptr = output->ripping_queue.next;
        output_format_ptr = list_entry(node_ptr, scarletbook_output_format_t, siblings);
        list_del(node_ptr);
        free(output_format_ptr->filename);
        free(output_format_ptr->working_filename);
        free(output_format_ptr);
    }
}

int scarletbook_output_enqueue_track(scarletbook_output_t *output, int area, int track, char *file_path, char *fmt, int dsd_encoded_export)
{
    scarletbook_format_handler_t const * handler;
    scarletbook_output_format_t * output_format_ptr;
    scarletbook_handle_t *sb_handle = output->sb_handle;

    if ((handler = find_output_format(fmt)))
    {
        output_format_ptr = calloc(sizeof(scarletbook_output_format_t), 1);
        output_format_ptr->sb_handle = sb_handle;
        init_output_format(output, output_format_ptr);
        output_format_ptr->cb_fwprintf = output->fwprintf_callback;
        output_format_ptr->area = area;
        output_format_ptr->track = track;
        output_format_ptr->handler = *handler;
        output_format_ptr->filename = strdup(file_path);
        output_format_ptr->channel_count = sb_handle->area[area].area_toc->channel_count;
        output_format_ptr->dst_encoded_import = sb_handle->area[area].area_toc->frame_format == FRAME_FORMAT_DST;
        output_format_ptr->dsd_encoded_export = dsd_encoded_export;


        if (handler->flags & OUTPUT_FLAG_EDIT_MASTER)
        {
            output_format_ptr->start_lsn = sb_handle->area[area].area_toc->track_start;
            output_format_ptr->length_lsn = sb_handle->area[area].area_toc->track_end - sb_handle->area[area].area_toc->track_start + 1;
        }
        else
        {
            // Read tracks without pauses
            //if(sb_handle->audio_frame_trimming)
            //{
            //    output_format_ptr->start_lsn = sb_handle->area[area].area_tracklist_offset->track_start_lsn[track];
            //    output_format_ptr->length_lsn = sb_handle->area[area].area_tracklist_offset->track_length_lsn[track];
            //}
            //else //// Read all LSNs (including pauses)
            //{
                if (track > 0)
                {
                    output_format_ptr->start_lsn = sb_handle->area[area].area_tracklist_offset->track_start_lsn[track];
                }
                else
                {
                    output_format_ptr->start_lsn = sb_handle->area[area].area_toc->track_start;
                }
                if (track < sb_handle->area[area].area_toc->track_count - 1)
                {
                    output_format_ptr->length_lsn = sb_handle->area[area].area_tracklist_offset->track_start_lsn[track + 1] - output_format_ptr->start_lsn + 1;
                }
                else
                {
                    output_format_ptr->length_lsn = sb_handle->area[area].area_toc->track_end - output_format_ptr->start_lsn + 1;
                }
            //}

            // DEBUG
            //output_format_ptr->cb_fwprintf(stderr, L"\n Debug: Queuing: track %d, area_tracklist_offset->track_start_lsn: %d, area_tracklist_offset->track_length_lsn: %d\n", track,
            //                               sb_handle->area[area].area_tracklist_offset->track_start_lsn[track], output_format_ptr->length_lsn = sb_handle->area[area].area_tracklist_offset->track_length_lsn[track]);
            //output_format_ptr->cb_fwprintf(stderr, L"\n Debug: Queuing: track %d, start_lsn: %d, length_lsn: %d\n", track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);

            // Do some integrity checks
            //The Track_Start_Address of a Track must be in the Track Area of
            //    the corresponding Audio Area
            if (!(output_format_ptr->start_lsn >= sb_handle->area[area].area_toc->track_start) ||
                !(output_format_ptr->start_lsn <= sb_handle->area[area].area_toc->track_end)    )
            {
                LOG(lm_output, LOG_NOTICE, ("Queuing error: track_start_lsn is not is area! area: %d, track %d, start_lsn: %d, length_lsn: %d", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn));
                output_format_ptr->cb_fwprintf(stderr, L"\n Queuing error: track_start_lsn is not is area! area: %d, track %d, start_lsn: %d, length_lsn: %d\n", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);
            }

            //  For 1 ≤ track < N_Tracks the following equation must be true:
            //        Track_Start_Address[track+1] ≥ Track_Start_Address[track] + Track_Length[track] - 1
            if (track < sb_handle->area[area].area_toc->track_count - 1)
            {
               if( !(sb_handle->area[area].area_tracklist_offset->track_start_lsn[track + 1] >= sb_handle->area[area].area_tracklist_offset->track_start_lsn[track] + sb_handle->area[area].area_tracklist_offset->track_length_lsn[track] - 1))
                 {
                     LOG(lm_output, LOG_NOTICE, ("Queuing error: equation not valid! area: %d, track %d, start_lsn: %d, length_lsn: %d", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn));
                     output_format_ptr->cb_fwprintf(stderr, L"\n Queuing error: equation not valid(beetween track_start_lsn and track_length_lsn)! area: %d, track %d, start_lsn: %d, length_lsn: %d\n", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);
                 }
            }
            else
            {
                if (!(sb_handle->area[area].area_toc->track_end >= sb_handle->area[area].area_tracklist_offset->track_start_lsn[track] + sb_handle->area[area].area_tracklist_offset->track_length_lsn[track] - 1))
                {
                    LOG(lm_output, LOG_NOTICE, ("Queuing error: equation not valid! area: %d, track %d, start_lsn: %d, length_lsn: %d", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn));
                    output_format_ptr->cb_fwprintf(stderr, L"\n Queuing error: equation not valid(beetween track_start_lsn and track_length_lsn)! area: %d, track %d, start_lsn: %d, length_lsn: %d\n", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);
                }
            }
        }

        LOG(lm_output, LOG_NOTICE, ("Queuing: %s, area: %d, track %d, start_lsn: %d, length_lsn: %d, dst_encoded_import: %d, dsd_encoded_export: %d", file_path, area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn, output_format_ptr->dst_encoded_import, output_format_ptr->dsd_encoded_export));

        list_add_tail(&output_format_ptr->siblings, &output->ripping_queue);

        return 0;
    }
    return -1;
}

int scarletbook_output_enqueue_raw_sectors(scarletbook_output_t *output, int start_lsn, int length_lsn, char *file_path, char *fmt)
{
    scarletbook_format_handler_t const * handler;
    scarletbook_output_format_t * output_format_ptr;
    scarletbook_handle_t *sb_handle = output->sb_handle;

    if ((handler = find_output_format(fmt)))
    {
        output_format_ptr = calloc(sizeof(scarletbook_output_format_t), 1);
        output_format_ptr->sb_handle = sb_handle;
        init_output_format(output, output_format_ptr);
        output_format_ptr->cb_fwprintf = output->fwprintf_callback;
        output_format_ptr->handler = *handler;
        output_format_ptr->filename = strdup(file_path);
        output_format_ptr->start_lsn = start_lsn;
        output_format_ptr->length_lsn = length_lsn;

        LOG(lm_output, LOG_NOTICE, ("Queuing raw: %s, start_lsn: %d, length_lsn: %d", file_path, start_lsn, length_lsn));

        list_add_tail(&output_format_ptr->siblings, &output->ripping_queue);

        return 0;
    }
    return -1;
}

int scarletbook_output_enqueue_concatenate_tracks(scarletbook_output_t *output, int area, int track, char *file_path, char *fmt, int dsd_encoded_export, int last_track)
{
    scarletbook_format_handler_t const *handler;
    scarletbook_output_format_t *output_format_ptr;
    scarletbook_handle_t *sb_handle = output->sb_handle;


    if ((handler = find_output_format(fmt)))
    {
        output_format_ptr = calloc(sizeof(scarletbook_output_format_t), 1);
        output_format_ptr->sb_handle = sb_handle;
        init_output_format(output, output_format_ptr);
        output_format_ptr->cb_fwprintf = output->fwprintf_callback;
        output_format_ptr->area = area;
        output_format_ptr->track = track;
        output_format_ptr->handler = *handler;
        output_format_ptr->filename = strdup(file_path);
        output_format_ptr->channel_count = sb_handle->area[area].area_toc->channel_count;
        output_format_ptr->dst_encoded_import = sb_handle->area[area].area_toc->frame_format == FRAME_FORMAT_DST;
        output_format_ptr->dsd_encoded_export = dsd_encoded_export;

        // read with pauses only
        // find the start lsn
        if (track > 0)
        {
            output_format_ptr->start_lsn = sb_handle->area[area].area_tracklist_offset->track_start_lsn[track];
        }
        else
        {
            output_format_ptr->start_lsn = sb_handle->area[area].area_toc->track_start;
        }

        if (last_track < sb_handle->area[area].area_toc->track_count - 1)
        {
            output_format_ptr->length_lsn = sb_handle->area[area].area_tracklist_offset->track_start_lsn[last_track + 1] - output_format_ptr->start_lsn + 1;
        }
        else
        {
            output_format_ptr->length_lsn = sb_handle->area[area].area_toc->track_end - output_format_ptr->start_lsn + 1;
        }

        // DEBUG
        //output_format_ptr->cb_fwprintf(stderr, L"\n Debug: Queuing concatenation: track %d, start_lsn: %d, length_lsn: %d\n", track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);

        // Do some integrity checks
        //The Track_Start_Address of a Track must be in the Track Area of
        //    the corresponding Audio Area
        if (!(output_format_ptr->start_lsn >= sb_handle->area[area].area_toc->track_start) ||
            !(output_format_ptr->start_lsn <= sb_handle->area[area].area_toc->track_end))
        {
            LOG(lm_output, LOG_NOTICE, ("Queuing error: track_start_lsn is not is area! area: %d, track %d, start_lsn: %d, length_lsn: %d", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn));
            output_format_ptr->cb_fwprintf(stderr, L"\n Queuing error: track_start_lsn is not is area! area: %d, track %d, start_lsn: %d, length_lsn: %d\n", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);
            }

            //  For 1 ≤ track < N_Tracks the following equation must be true:
            //        Track_Start_Address[track+1] ≥ Track_Start_Address[track] + Track_Length[track] - 1
            if (track < sb_handle->area[area].area_toc->track_count - 1)
            {
                if (!(sb_handle->area[area].area_tracklist_offset->track_start_lsn[track + 1] >= sb_handle->area[area].area_tracklist_offset->track_start_lsn[track] + sb_handle->area[area].area_tracklist_offset->track_length_lsn[track] - 1))
                {
                    LOG(lm_output, LOG_NOTICE, ("Queuing error: equation not valid! area: %d, track %d, start_lsn: %d, length_lsn: %d", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn));
                    output_format_ptr->cb_fwprintf(stderr, L"\n Queuing error: equation not valid(beetween track_start_lsn and track_length_lsn)! area: %d, track %d, start_lsn: %d, length_lsn: %d\n", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);
                }
            }
            else
            {
                if (!(sb_handle->area[area].area_toc->track_end >= sb_handle->area[area].area_tracklist_offset->track_start_lsn[track] + sb_handle->area[area].area_tracklist_offset->track_length_lsn[track] - 1))
                {
                    LOG(lm_output, LOG_NOTICE, ("Queuing error: equation not valid! area: %d, track %d, start_lsn: %d, length_lsn: %d", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn));
                    output_format_ptr->cb_fwprintf(stderr, L"\n Queuing error: equation not valid(beetween track_start_lsn and track_length_lsn)! area: %d, track %d, start_lsn: %d, length_lsn: %d\n", area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn);
                }
            }


        LOG(lm_output, LOG_NOTICE, ("Queuing: concatenation %s, area: %d, track %d, start_lsn: %d, length_lsn: %d, dst_encoded_import: %d, dsd_encoded_export: %d", file_path, area, track, output_format_ptr->start_lsn, output_format_ptr->length_lsn, output_format_ptr->dst_encoded_import, output_format_ptr->dsd_encoded_export));

        list_add_tail(&output_format_ptr->siblings, &output->ripping_queue);

        return 0;
    }
    return -1;
}

static int create_output_file(scarletbook_output_format_t *ft)
{
    int result;
    char inprogress[MAX_BUFF_FULL_PATH_LEN];

    if (output_path_with_marker(ft->filename, "inprogress", inprogress, sizeof(inprogress)) != 0)
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE,
                                            "output path is too long");
        return -1;
    }
    ft->working_filename = strdup(inprogress);
    if (!ft->working_filename)
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE, "out of memory");
        return -1;
    }

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    char filename_long[MAX_BUFF_FULL_PATH_LEN];
	memset(filename_long, '\0', MAX_BUFF_FULL_PATH_LEN);
    strcpy(filename_long,"\\\\?\\");
    strncat(filename_long,ft->working_filename, MAX_BUFF_FULL_PATH_LEN - 8);

    wchar_t *wide_filename;

    CHAR2WCHAR(wide_filename,filename_long);
    ft->fd = _wfopen(wide_filename, L"wb");

    free(wide_filename);
#else
    ft->fd = fopen(ft->working_filename, "wb");
#endif
    if (ft->fd == NULL)
    {
        LOG(lm_output, LOG_ERROR, ("error creating %s, errno: %d, %s", ft->filename, errno, strerror(errno)));
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE, strerror(errno));
        goto error;
    }

    /*
     * DSF writes are already block-sized.  Do not add a long-lived userspace
     * cache here: corrupted cache contents used to be flushed as complete
     * 1 MiB regions while the resulting file remained structurally valid.
     * The operating system's file cache still coalesces these writes.
     */
    if (setvbuf(ft->fd, NULL, _IONBF, 0) != 0)
    {
        LOG(lm_output, LOG_ERROR, ("error disabling stdio buffering for %s", ft->filename));
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE,
                                            "unable to configure output stream");
        fclose(ft->fd);
        ft->fd = NULL;
        goto error;
    }

    ft->priv = calloc(1, ft->handler.priv_size);

    result = ft->handler.startwrite ? (*ft->handler.startwrite)(ft) : 0;
    if (result != 0 && ft->error_number == SACD_OUTPUT_ERROR_NONE)
    {
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_CREATE,
                                            "unable to initialize output container");
    }

    return result;

error:

    // close_output_file will be called
    return -1;
}

static int publish_output_file(scarletbook_output_format_t *ft, const char *marker)
{
    char published[MAX_BUFF_FULL_PATH_LEN];
    const char *destination = ft->filename;
    if (marker)
    {
        if (output_path_with_marker(ft->filename, marker, published, sizeof(published)) != 0)
            return -1;
        destination = published;
    }
    if (!ft->working_filename || output_publish_atomic(ft->working_filename, destination) != 0)
    {
        char error[256];
        snprintf(error, sizeof(error), "rename failed: %s", strerror(errno));
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_FINALIZE, error);
        return -1;
    }
    LOG(lm_output, marker ? LOG_WARNING : LOG_INFO,
        ("published output path=%s status=%s defects=%u", destination,
         marker ? marker : "clean", atomic_load(&ft->media_error_count)));
    return 0;
}

static inline int close_output_file(scarletbook_output_format_t *ft, const char *marker)
{
    int result=0;
    int had_file = ft->fd != NULL;

	if(ft->fd != NULL){
		result = ft->handler.stopwrite ? (*ft->handler.stopwrite)(ft) : 0;
		if(result ==-1)
			LOG(lm_output, LOG_ERROR, ("error closing %s", ft->filename));
	}

    if (ft->fd != NULL)
    {
        if (fclose(ft->fd) != 0)
            result = -1;
        ft->fd = NULL;
    }

    if (result != 0)
    {
        marker = "failed";
        scarletbook_output_format_set_error(ft, SACD_OUTPUT_ERROR_FINALIZE,
                                            "container finalization failed");
    }
    if (had_file && ft->working_filename && publish_output_file(ft, marker) != 0)
        result = -1;

    if(ft->filename)free(ft->filename);
    if(ft->working_filename)free(ft->working_filename);
    if(ft->priv)free(ft->priv);
    free(ft);

    return result;
}

static void scarletbook_output_init_stats(scarletbook_output_t *output)
{
    struct list_head * node_ptr;
    scarletbook_output_format_t * output_format_ptr;

    output->stats_total_sectors = 0;
    output->stats_total_sectors_processed = 0;
    output->stats_current_file_total_sectors = 0;
    output->stats_current_file_sectors_processed = 0;
    output->stats_current_track = 0;
    output->stats_total_tracks = 0;

    list_for_each(node_ptr, &output->ripping_queue)
    {
        output_format_ptr = list_entry(node_ptr, scarletbook_output_format_t, siblings);
        output->stats_total_sectors += output_format_ptr->length_lsn;
        output->stats_total_tracks++;
    }
}

static inline int write_block(scarletbook_output_format_t * ft, const uint8_t *buf, size_t len)
{
    int actual = ft->handler.write? (*ft->handler.write)(ft, buf, len) : 0;
    if (actual < 0 ) return -1;
    ft->write_length += actual;
    return actual;
}

static void set_fatal_error(scarletbook_output_format_t *ft,
                            scarletbook_output_error_t error_number,
                            const char *message)
{
    scarletbook_output_format_set_error(ft, error_number,
                                        message ? message : "fatal output error");
    atomic_store(&ft->abort_current, 1);
}

static int record_media_errors(scarletbook_output_format_t *ft, uint32_t count,
                               const char *kind, uint32_t lsn)
{
    uint32_t total;
    if (count == 0)
        return 0;
    {
        uint32_t previous = atomic_fetch_add(&ft->media_error_count, count);
        (void)media_error_budget_add(previous, count, ft->max_media_errors, &total);
    }
    ft->partial = 1;
    ft->error_number = SACD_OUTPUT_ERROR_MEDIA;
    snprintf(ft->error_str, sizeof(ft->error_str), "%s at LSN %u", kind, lsn);
    LOG(lm_output, LOG_WARNING,
        ("media defect kind=%s lsn=%u count=%u total=%u limit=%u file=%s",
         kind, lsn, count, total, ft->max_media_errors, ft->filename));
    if (total > ft->max_media_errors)
    {
        atomic_store(&ft->abort_current, 1);
        LOG(lm_output, LOG_ERROR,
            ("media defect budget exceeded total=%u limit=%u file=%s",
             total, ft->max_media_errors, ft->filename));
        return -1;
    }
    return 0;
}

static void frame_decoded_callback(uint8_t* frame_data, size_t frame_size, void *userdata)
{
    scarletbook_output_format_t *ft = (scarletbook_output_format_t *) userdata;
    int rezult = write_block(ft, frame_data, frame_size);
    if (rezult == -1)
    {
	 ft->cb_fwprintf(stderr, L"\n ERROR in frame_decoded_callback():write_block()...at writting in file.\n");
	 LOG(lm_output, LOG_ERROR, ("ERROR in frame_decoded_callback():write_block()...writting in file: %s  ",ft->filename) );
	 set_fatal_error(ft, SACD_OUTPUT_ERROR_WRITE, "decoded frame write failed");
	}
}

static void frame_error_callback(int frame_count, int frame_error_code, const char *frame_error_message, void *userdata)
{
    scarletbook_output_format_t *ft = (scarletbook_output_format_t *) userdata;

    wchar_t *wide_frame_error_mesage;
    CHAR2WCHAR(wide_frame_error_mesage, frame_error_message);
    ft->cb_fwprintf(stderr, L"\n ERROR in dst_decoder: %s in frame: %d\n", wide_frame_error_mesage, frame_count); //frame_error_message
    free(wide_frame_error_mesage);
    LOG(lm_output, LOG_ERROR, ("ERROR in dst_decoder: %s in frame: %d", frame_error_message, frame_count));
    (void)record_media_errors(ft, 1, "DST decode failure", ft->current_lsn);
}

static void frame_read_callback(scarletbook_handle_t *handle, uint8_t* frame_data, size_t frame_size, void *userdata)
{
    scarletbook_output_format_t *ft = (scarletbook_output_format_t *) userdata;


    if (ft->handler.flags & OUTPUT_FLAG_EDIT_MASTER) //  only for DSDIFF master
    {
        if (ft->dsd_encoded_export && ft->dst_encoded_import)
        {
            dst_decoder_decode(ft->dst_decoder, frame_data, frame_size);
			ft->sb_handle->count_frames++;
        }
        else
        {
            int rezult;
            rezult = write_block(ft, frame_data, frame_size);
            if (rezult == -1)
            {
                ft->cb_fwprintf(stderr, L"\n ERROR in frame_read_callback():write_block()..at writting in dsdiff master file. \n");
                LOG(lm_output, LOG_ERROR, ("ERROR in frame_read_callback:write_block()...writting in file: %s  ", ft->filename));
                set_fatal_error(ft, SACD_OUTPUT_ERROR_WRITE, "Edit Master frame write failed");
            }
			ft->sb_handle->count_frames++;
        }
    }
    else   // DSF, DSDIFF
    {
        if (ft->sb_handle->audio_frame_trimming > 0)  // (pausese will not be included)
        {
            uint32_t frame_count_time_start = TIME_FRAMECOUNT(&handle->area[ft->area].area_tracklist_time->start[ft->track]);
            uint32_t frame_count_time_end = frame_count_time_start +
                                            TIME_FRAMECOUNT(&handle->area[ft->area].area_tracklist_time->duration[ft->track]);
            //uint32_t frame_timecode = TIME_FRAMECOUNT(&handle->audio_sector.frame[handle->frame_info_idx].timecode);
            uint32_t frame_timecode = TIME_FRAMECOUNT(&handle->frame.timecode);

            if (frame_timecode >= frame_count_time_start &&
                frame_timecode < frame_count_time_end)
            {
                if (ft->dsd_encoded_export && ft->dst_encoded_import)
                {
                    dst_decoder_decode(ft->dst_decoder, frame_data, frame_size);
                    ft->sb_handle->count_frames++;
                }
                else
                {
                    int rezult;
                    rezult = write_block(ft, frame_data, frame_size);
                    if (rezult == -1)
                    {
                        ft->cb_fwprintf(stderr, L"\n ERROR in frame_read_callback():write_block()..at writting in dsf/dsdiff file. \n");
                        LOG(lm_output, LOG_ERROR, ("ERROR in frame_read_callback:write_block()...writting in file: %s  ", ft->filename));
                        set_fatal_error(ft, SACD_OUTPUT_ERROR_WRITE, "audio frame write failed");
                    }
                    ft->sb_handle->count_frames++;
                }
            }
        }
        else  // no audioframe trimming (pauses will be included)
        {
            if (ft->dsd_encoded_export && ft->dst_encoded_import)
            {
                dst_decoder_decode(ft->dst_decoder, frame_data, frame_size);
                ft->sb_handle->count_frames++;
            }
            else
            {
                int rezult;
                rezult = write_block(ft, frame_data, frame_size);
                if (rezult == -1)
                {
                    ft->cb_fwprintf(stderr, L"\n ERROR in frame_read_callback():write_block()..at writting in file. \n");
                    LOG(lm_output, LOG_ERROR, ("ERROR in frame_read_callback:write_block()...writting in file: %s  ", ft->filename));
                    set_fatal_error(ft, SACD_OUTPUT_ERROR_WRITE, "audio frame write failed");
                }
                ft->sb_handle->count_frames++;
            }
        }

    }
}

static sacd_input_read_result_t output_read_callback(void *userdata, uint32_t lsn,
                                                     uint32_t blocks, uint8_t *buffer)
{
    return sacd_read_block_raw_ex((sacd_reader_t *)userdata, lsn, blocks, buffer);
}

static int process_audio_runs(scarletbook_output_format_t *ft, uint8_t *buffer,
                              const uint8_t *valid_map, uint32_t blocks,
                              uint32_t base_lsn, int final_batch)
{
    scarletbook_handle_t *handle = ft->sb_handle;
    for (uint32_t index = 0; index < blocks; ++index)
    {
        if (!valid_map[index])
        {
            handle->frame.started = 0;
            handle->frame.size = 0;
            if (ft->handler.discontinuity)
                ft->handler.discontinuity(ft);
            continue;
        }
        int parser_result = scarletbook_process_frames(handle,
                            buffer + index * SACD_LSN_SIZE, 1,
                            final_batch && index + 1 == blocks, frame_read_callback, ft);
        if (parser_result < 0)
        {
            uint32_t defects = (uint32_t)(-parser_result);
            if (record_media_errors(ft, defects, "malformed audio sector/frame",
                                    base_lsn + index) != 0)
                return -1;
        }
        if (atomic_load(&ft->abort_current))
            return -1;
    }
    return 0;
}

static void *processing_thread(void *arg)
{
    scarletbook_output_t *output = (scarletbook_output_t *) arg;
    scarletbook_handle_t *handle = output->sb_handle;
    struct list_head * node_ptr;
    scarletbook_output_format_t *ft = NULL;
	int no_tracks_with_errors = 0;

    atomic_store(&output->processing, 1);
    while (!list_empty(&output->ripping_queue))
    {
        node_ptr = output->ripping_queue.next;

        ft = list_entry(node_ptr, scarletbook_output_format_t, siblings);
        list_del(node_ptr);

        if (ft->dsd_encoded_export && ft->dst_encoded_import)
        {
            ft->dst_decoder = dst_decoder_create(ft->channel_count, frame_decoded_callback, frame_error_callback, ft);
        }

        output->stats_current_file_total_sectors = ft->length_lsn;
        output->stats_current_file_sectors_processed = 0;
        output->stats_current_track++;

        if (output->stats_track_callback)
        {
            output->stats_track_callback(ft->filename, output->stats_current_track, output->stats_total_tracks);
        }

        scarletbook_frame_init(handle);
        handle->count_frames = 0;

        if (create_output_file(ft) == 0)
        {
            uint32_t block_size=0, end_lsn=0;
            uint8_t valid_map[MAX_PROCESSING_BLOCK_SIZE];

            // what blocks do we need to process?
            ft->current_lsn = ft->start_lsn;
            end_lsn = ft->start_lsn + ft->length_lsn;

            //handle->count_frames = 0;

            while (atomic_load(&output->stop_processing) == 0)
            {
                if (ft->current_lsn < end_lsn)
                {
                    block_size = min(end_lsn - ft->current_lsn, MAX_PROCESSING_BLOCK_SIZE);

                    uint32_t batch_lsn = ft->current_lsn;
                    uint32_t current_defects = atomic_load(&ft->media_error_count);
                    uint32_t remaining = current_defects <= ft->max_media_errors
                                             ? ft->max_media_errors - current_defects : 0;
                    uint32_t hole_limit = remaining == UINT32_MAX ? UINT32_MAX : remaining + 1;
                    media_recovery_result_t recovery = media_read_recover_limited(
                        output_read_callback, ft->sb_handle->sacd, batch_lsn, block_size,
                        SACD_LSN_SIZE, output->read_buffer, valid_map, hole_limit);
                    if (recovery.fatal)
                    {
                        set_fatal_error(ft, SACD_OUTPUT_ERROR_INPUT_FATAL, recovery.error_string);
                        output->result = SACD_OUTPUT_RESULT_FATAL;
                        break;
                    }
                    if (recovery.holes)
                    {
                        uint32_t recorded_holes = 0;
                        int budget_exceeded = 0;
                        for (uint32_t index = 0;
                             index < block_size && recorded_holes < recovery.holes;
                             ++index)
                        {
                            if (!valid_map[index])
                            {
                                recorded_holes++;
                                if (record_media_errors(ft, 1, "unreadable sector",
                                                        batch_lsn + index) != 0)
                                {
                                    budget_exceeded = 1;
                                    break;
                                }
                            }
                        }
                        if (budget_exceeded)
                        {
                            output->result = SACD_OUTPUT_RESULT_PARTIAL;
                            break;
                        }
                    }

                    ft->current_lsn += block_size;
                    output->stats_total_sectors_processed += block_size;
                    output->stats_current_file_sectors_processed += block_size;

                    //debug
                    //output->fwprintf_callback(stdout, L"\n \n Debug - scarletbook_process_frames(): block_size %d, last bloc=%d \n", block_size, ft->current_lsn == end_lsn);

                    // process DSD & DST frames
                    if (ft->handler.flags & OUTPUT_FLAG_DSD || ft->handler.flags & OUTPUT_FLAG_DST)
                    {
                       (void)process_audio_runs(ft, output->read_buffer, valid_map, block_size, batch_lsn,
                                                ft->current_lsn >= end_lsn);
                       if (ft->current_lsn >= end_lsn){
                           LOG(lm_output, LOG_NOTICE, ("End track no. %d. After last call to scarletbook_process_frames. current_lsn >= end_lsn, current_lsn:%d, end_lsn:%d, block_size:%d", ft->track, ft->current_lsn, end_lsn, block_size));
                           uint32_t frame_count_time_start = TIME_FRAMECOUNT(&handle->area[ft->area].area_tracklist_time->start[ft->track]);
                           uint32_t frame_count_time_end = frame_count_time_start +  TIME_FRAMECOUNT(&handle->area[ft->area].area_tracklist_time->duration[ft->track]);
                           LOG(lm_output, LOG_NOTICE, ("End track. After last call to scarletbook_process_frames. frame_count_time_start:%u, frame_count_time_end:%u", frame_count_time_start, frame_count_time_end));
                       }
                    }
                    // ISO output is written without frame processing
                    else if (ft->handler.flags & OUTPUT_FLAG_RAW)
                    {
                       size_t rezult=  write_block(ft, output->read_buffer, block_size);
					   if (rezult ==(size_t) -1)
					   {
                               output->fwprintf_callback(stdout, L"\n \n Error in writting ISO in file. \n");
                               set_fatal_error(ft, SACD_OUTPUT_ERROR_WRITE, "ISO write failed");
                               output->result = SACD_OUTPUT_RESULT_FATAL;
						   }

                    }
                    if (atomic_load(&ft->abort_current))
                        break;

                    // debug
                    //output->fwprintf_callback(stdout, L"\n \n After scarlet_processe_frames. Processed: %d audioframes\n", ft->count_frames);

                    // update statistics
                    if (output->stats_progress_callback)
                    {
                        output->stats_progress_callback(output->stats_total_sectors, output->stats_total_sectors_processed,
                            output->stats_current_file_total_sectors, output->stats_current_file_sectors_processed);
                    }
                }
                else
                {
                    break;
                } // end if (ft->current_lsn < end_lsn)

            } // end while processing is active

        }  // end  if (create_output_file(ft)
        else  // error in creating file
        {
				no_tracks_with_errors++;
            output->result = SACD_OUTPUT_RESULT_FATAL;
            output->fwprintf_callback(stdout, L"\n \n ERROR: Cannot create output file for current track number %d of total %d !!", output->stats_current_track, output->stats_total_tracks);
            LOG(lm_output, LOG_ERROR, ("ERROR: Cannot create output file for current track number %d of total %d !!", output->stats_current_track, output->stats_total_tracks));
        }

        // Show statistics only for DFF-edit-master : print Error if nr of processed frames < of duration (nr of frames)
        if (ft->handler.flags & OUTPUT_FLAG_EDIT_MASTER)
        {
            int count_sec = (int)(handle->count_frames / SACD_FRAME_RATE);
            uint32_t duration = (uint32_t)TIME_FRAMECOUNT(&handle->area[ft->area].area_toc->total_playtime);
            output->fwprintf_callback(stdout, L"\n \n Processed %d audioframes (%02d:%02d:%02d [mins:secs:frames]). Total playing time specified:%d (%02d:%02d:%02d [mins:secs:frames])\n",
                                      handle->count_frames,
                                      (int)count_sec / 60,
                                      (int)count_sec % 60,
                                      (int)handle->count_frames % SACD_FRAME_RATE,
                                      duration,
                                      handle->area[ft->area].area_toc->total_playtime.minutes,
                                      handle->area[ft->area].area_toc->total_playtime.seconds,
                                      handle->area[ft->area].area_toc->total_playtime.frames);
            if (handle->count_frames < duration)
            {
                LOG(lm_output, LOG_NOTICE, ("Warning: Number of processed audioframes (%d) is smaller than number of frames in duration (%d)", handle->count_frames, duration));
                output->fwprintf_callback(stdout, L"\n \n Warning: Number of processed audioframes (%d) is smaller than number of frames in duration (%d) \n", handle->count_frames, duration);
            }
        }
        else
        // Show statistics only for DSF/DFF : print Error if nr of processed frames < of duration (nr of frames)
        if (ft->handler.flags & OUTPUT_FLAG_DSD || ft->handler.flags & OUTPUT_FLAG_DST )
        {
            if(handle->concatenate == 0)
            {
                uint32_t duration = (uint32_t)TIME_FRAMECOUNT(&handle->area[ft->area].area_tracklist_time->duration[ft->track]);

                output->fwprintf_callback(stdout, L"\n \n Processed %d audioframes. Duration specified: %d (%02d:%02d:%02d [mins:secs:frames])\n",
                                          handle->count_frames, duration,
                                          handle->area[ft->area].area_tracklist_time->duration[ft->track].minutes,
                                          handle->area[ft->area].area_tracklist_time->duration[ft->track].seconds,
                                          handle->area[ft->area].area_tracklist_time->duration[ft->track].frames);
                if (handle->count_frames < duration) //output->stats_current_count_frames
                {
                    LOG(lm_output, LOG_NOTICE, ("Warning: Number of processed audioframes (%d) is smaller than number of frames in duration (%d)", handle->count_frames, duration));
                    output->fwprintf_callback(stdout, L"\n \n Warning: Number of processed audioframes (%d) is smaller than number of frames in duration (%d) \n", handle->count_frames, duration);
                }
            }
            else
            {
                int count_sec = (int)(handle->count_frames / SACD_FRAME_RATE);
                output->fwprintf_callback(stdout, L"\n \n Processed %d audioframes. Total duration: %02d:%02d:%02d [mins:secs:frames] \n",
                                          handle->count_frames,
                                          (int)count_sec / 60,
                                          (int)count_sec % 60,
                                          (int)handle->count_frames % SACD_FRAME_RATE);
            }

        }

        if (atomic_load(&output->stop_processing) == 1)
        {
            output->fwprintf_callback(stdout, L"\n ...stop processing\n");
            LOG(lm_output, LOG_WARNING, ("processing interrupted by user"));
            output->interrupted = 1;
            output->result = SACD_OUTPUT_RESULT_INTERRUPTED;
            ft->partial = 1;
            ft->error_number = SACD_OUTPUT_ERROR_INTERRUPTED;
            snprintf(ft->error_str, sizeof(ft->error_str), "interrupted by user");
        }

		//DEBUG LOG(lm_output, LOG_ERROR, ("before dsd_encoded_export"));

        if (ft->dsd_encoded_export && ft->dst_encoded_import)
        {
            dst_decoder_destroy(ft->dst_decoder);
        }

		//DEBUG LOG(lm_output, LOG_ERROR, ("before close_output_file"));

        {
            const char *marker = NULL;
            if (ft->error_number >= SACD_OUTPUT_ERROR_CREATE &&
                ft->error_number <= SACD_OUTPUT_ERROR_INPUT_FATAL)
                output->result = SACD_OUTPUT_RESULT_FATAL;
            if (output->result == SACD_OUTPUT_RESULT_FATAL ||
                (ft->error_number >= SACD_OUTPUT_ERROR_CREATE &&
                 ft->error_number <= SACD_OUTPUT_ERROR_INPUT_FATAL))
                marker = "failed";
            else if (ft->partial || atomic_load(&ft->media_error_count) > 0 || output->interrupted)
                marker = "partial";

            if (marker && strcmp(marker, "partial") == 0 &&
                output->result == SACD_OUTPUT_RESULT_CLEAN)
                output->result = SACD_OUTPUT_RESULT_PARTIAL;
            if (close_output_file(ft, marker) != 0)
                output->result = SACD_OUTPUT_RESULT_FATAL;
        }
        if (!sacd_output_queue_continues(output->result) || output->interrupted)
            break;
    } // end while (!list_empty(&output->ripping_queue))

    if (no_tracks_with_errors > 0)
    {
        output->fwprintf_callback(stdout, L"\n \n Error: %d track(s) has errors of total %d tracks !!", no_tracks_with_errors, output->stats_total_tracks);
    }

	// DEBUG LOG(lm_output, LOG_ERROR, ("before destroy_ripping_queue"));

    destroy_ripping_queue(output);
    atomic_store(&output->processing, 0);
    return NULL;
}

scarletbook_output_t *scarletbook_output_create(scarletbook_handle_t *handle, stats_track_callback_t cb_track, stats_progress_callback_t cb_progress, fwprintf_callback_t cb_fwprintf)
{
    scarletbook_output_t *output = (scarletbook_output_t *) calloc(1, sizeof(scarletbook_output_t));

    atomic_init(&output->stop_processing, 0);
    atomic_init(&output->processing, 0);
    INIT_LIST_HEAD(&output->ripping_queue);
    output->read_buffer = (uint8_t *) malloc(MAX_PROCESSING_BLOCK_SIZE * SACD_LSN_SIZE);
    output->sb_handle = handle;
    output->stats_track_callback = cb_track;
    output->stats_progress_callback = cb_progress;
    output->fwprintf_callback = cb_fwprintf;
    output->max_media_errors = 10;
    output->flac_sample_rate = 88200;
    output->result = SACD_OUTPUT_RESULT_CLEAN;

    return output;
}

int scarletbook_output_is_busy(scarletbook_output_t *output)
{
    return atomic_load(&output->processing);
}

int scarletbook_output_start(scarletbook_output_t *output)
{
    int ret = 0;

    scarletbook_output_init_stats(output);

    ret = pthread_create(&output->processing_thread_id, NULL, processing_thread, (void *) output);
    if (ret)
    {
        LOG(lm_output, LOG_ERROR, ("return code from processing thread creation is %d\n", ret));
    }

    return ret;
}

void scarletbook_output_interrupt(scarletbook_output_t *output)
{
    if (output)
        atomic_store(&output->stop_processing, 1);
}

void scarletbook_output_set_max_read_errors(scarletbook_output_t *output, uint32_t maximum)
{
    struct list_head *node_ptr;
    if (!output)
        return;
    output->max_media_errors = maximum;
    list_for_each(node_ptr, &output->ripping_queue)
    {
        scarletbook_output_format_t *ft = list_entry(node_ptr, scarletbook_output_format_t, siblings);
        ft->max_media_errors = maximum;
    }
}

void scarletbook_output_set_flac_sample_rate(scarletbook_output_t *output,
                                             unsigned int sample_rate)
{
    struct list_head *node_ptr;
    if (!output)
        return;
    output->flac_sample_rate = sample_rate;
    list_for_each(node_ptr, &output->ripping_queue)
    {
        scarletbook_output_format_t *ft =
            list_entry(node_ptr, scarletbook_output_format_t, siblings);
        ft->flac_sample_rate = sample_rate;
    }
}

int scarletbook_output_result(scarletbook_output_t *output)
{
    return output ? output->result : SACD_OUTPUT_RESULT_FATAL;
}

int scarletbook_output_destroy(scarletbook_output_t *output)
{
    void *thr_exit_code;
    int ret = 0;

    if (!output)
        return -1;

    ret = pthread_join(output->processing_thread_id, &thr_exit_code);
    if (ret != 0)
    {
        LOG(lm_output, LOG_ERROR, ("processing thread didn't close properly... %p", thr_exit_code));
    }

    // If decoding is aborted (eg. ctrl+C), then free() buffers after the decoder has been destroyed,
    // to ensure that buffers aren't still in use when they're free()d.
    if (ret != 0)
        output->result = SACD_OUTPUT_RESULT_FATAL;
    ret = output->result;
    free(output->read_buffer);
    free(output);

    return ret;
}
