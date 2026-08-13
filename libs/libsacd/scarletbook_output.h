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

#ifndef SCARLETBOOK_OUTPUT_H_INCLUDED
#define SCARLETBOOK_OUTPUT_H_INCLUDED

#include <stddef.h>
#include <stdio.h>

#include <dst_decoder.h>
#include <stdatomic.h>

#include "scarletbook.h"

// forward declaration
typedef struct scarletbook_output_format_t scarletbook_output_format_t;
typedef struct scarletbook_output_s scarletbook_output_t;

typedef enum scarletbook_output_error_e
{
    SACD_OUTPUT_ERROR_NONE = 0,
    SACD_OUTPUT_ERROR_MEDIA = 1,
    SACD_OUTPUT_ERROR_CREATE = 2,
    SACD_OUTPUT_ERROR_WRITE = 3,
    SACD_OUTPUT_ERROR_FINALIZE = 4,
    SACD_OUTPUT_ERROR_INPUT_FATAL = 5,
    SACD_OUTPUT_ERROR_INTERRUPTED = 6
} scarletbook_output_error_t;

typedef enum scarletbook_output_result_e
{
    SACD_OUTPUT_RESULT_CLEAN = 0,
    SACD_OUTPUT_RESULT_PARTIAL = 1,
    SACD_OUTPUT_RESULT_FATAL = 2,
    SACD_OUTPUT_RESULT_INTERRUPTED = 130
} scarletbook_output_result_t;

void scarletbook_output_format_set_error(scarletbook_output_format_t *format,
                                         scarletbook_output_error_t error_number,
                                         const char *error_string);

enum
{
    OUTPUT_FLAG_RAW         = 1 << 0,
    OUTPUT_FLAG_DSD         = 1 << 1,
    OUTPUT_FLAG_DST         = 1 << 2,
    OUTPUT_FLAG_EDIT_MASTER = 1 << 3
};

// Handler structure defined by each output format.
typedef struct scarletbook_format_handler_t
{
    char const *description;
    char const *name;
    int (*startwrite)(scarletbook_output_format_t *ft);
    int (*write)(scarletbook_output_format_t *ft, const uint8_t *buf, size_t len);
    int (*stopwrite)(scarletbook_output_format_t *ft);
    int         flags;
    size_t      priv_size;
}
scarletbook_format_handler_t;

typedef int (*fwprintf_callback_t)(FILE *stream, const wchar_t *format, ...);

struct scarletbook_output_format_t
{
    int                             area;
    int                             track;
    uint32_t                        start_lsn;
    uint32_t                        length_lsn;
    uint32_t                        current_lsn;
    char                           *filename;
    char                           *working_filename;

    int                             channel_count;

    FILE                           *fd;
    uint64_t                        write_length;
    uint64_t                        write_offset;

    int                             dst_encoded_import;
    int                             dsd_encoded_export;

    scarletbook_format_handler_t    handler;
    void                           *priv;

    int                             error_number;
    char                            error_str[256];
    uint32_t                        max_media_errors;
    atomic_uint                     media_error_count;
    atomic_int                      abort_current;
    int                             partial;

    dst_decoder_t                  *dst_decoder;

    scarletbook_handle_t           *sb_handle;
    fwprintf_callback_t             cb_fwprintf;

    struct list_head                siblings;
};

typedef void (*stats_progress_callback_t)(uint32_t stats_total_sectors, uint32_t stats_total_sectors_processed,
                                          uint32_t stats_current_file_total_sectors, uint32_t stats_current_file_sectors_processed);

typedef void (*stats_track_callback_t)(char *filename, int current_track, int total_tracks);

scarletbook_output_t *scarletbook_output_create(scarletbook_handle_t *, stats_track_callback_t, stats_progress_callback_t, fwprintf_callback_t);
int scarletbook_output_destroy(scarletbook_output_t *);
int scarletbook_output_enqueue_track(scarletbook_output_t *, int, int, char *, char *, int);
int scarletbook_output_enqueue_raw_sectors(scarletbook_output_t *, int, int, char *, char *);
int scarletbook_output_enqueue_concatenate_tracks(scarletbook_output_t *output, int area, int track, char *file_path, char *fmt, int dsd_encoded_export, int last_track);
int scarletbook_output_start(scarletbook_output_t *);
void scarletbook_output_set_max_read_errors(scarletbook_output_t *, uint32_t);
int scarletbook_output_result(scarletbook_output_t *);
void scarletbook_output_interrupt(scarletbook_output_t *);
int scarletbook_output_is_busy(scarletbook_output_t *);

#endif /* SCARLETBOOK_OUTPUT_H_INCLUDED */
