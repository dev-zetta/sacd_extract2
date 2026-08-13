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
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#if defined(WIN32)
#include <io.h>
#endif

#include <utils.h>
#include <logging.h>
#include <socket.h>
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <charset.h>

#include "scarletbook.h"
#include "sacd_input.h"
#include "sacd_pb_stream.h"
#include "sacd_ripper.pb.h"

sacd_input_t (*sacd_input_open)         (const char *);
int          (*sacd_input_close)        (sacd_input_t);
uint32_t     (*sacd_input_read)         (sacd_input_t, uint32_t, uint32_t, void *);
sacd_input_read_result_t (*sacd_input_read_ex)(sacd_input_t, uint32_t, uint32_t, void *);
char *       (*sacd_input_error)        (sacd_input_t);
uint32_t     (*sacd_input_total_sectors)(sacd_input_t);

struct sacd_input_s
{
    int                 fd;
    uint8_t            *input_buffer;
};

static sacd_input_read_result_t make_read_result(sacd_input_status_t status,
                                                  uint32_t blocks_read,
                                                  int error_number,
                                                  const char *error_string)
{
    sacd_input_read_result_t result;
    memset(&result, 0, sizeof(result));
    result.status = status;
    result.blocks_read = blocks_read;
    result.error_number = error_number;
    if (error_string)
        snprintf(result.error_string, sizeof(result.error_string), "%s", error_string);
    return result;
}

/**
 * initialize and open a SACD device or file.
 */
static sacd_input_t sacd_dev_input_open(const char *target)
{
    sacd_input_t dev;

    /* Allocate the library structure */
    dev = (sacd_input_t) calloc(sizeof(*dev), 1);
    if (dev == NULL)
    {
        fprintf(stderr, "libsacdread: Could not allocate memory.\n");
        return NULL;
    }

    /* Open the device */
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    wchar_t *wide_filename;

    CHAR2WCHAR(wide_filename, target);
    dev->fd = _wopen(wide_filename, O_RDONLY | O_BINARY);
    free(wide_filename);
#else
    dev->fd = open(target, O_RDONLY);
#endif


    if (dev->fd < 0)
    {
        goto error;
    }

    return dev;

error:

    free(dev);

    return 0;
}

/**
 * return the last error message
 */
static char *sacd_dev_input_error(sacd_input_t dev)
{
    /* use strerror(errno)? */
    return (char *) "unknown error";
}

/**
 * read data from the device.
 */
static sacd_input_read_result_t sacd_dev_input_read_ex(sacd_input_t dev, uint32_t pos,
                                                       uint32_t blocks, void *buffer)
{
    off_t ret_lseek;
    size_t len;
    ssize_t ret;

    ret_lseek = lseek(dev->fd, (off_t)pos * (off_t)SACD_LSN_SIZE, SEEK_SET);
    if (ret_lseek < 0)  // -1 on error
    {
		LOG(lm_input, LOG_ERROR, ("lseek failed lsn=%u errno=%d error=%s", pos, errno, strerror(errno)));
        return make_read_result(SACD_INPUT_FATAL, 0, errno, strerror(errno));
    }

    len = (size_t) blocks * SACD_LSN_SIZE;

    ret = read(dev->fd, buffer, len);

    if (ret < 0)
    {
        int saved_errno = errno;
        LOG(lm_input, LOG_ERROR, ("read failed lsn=%u blocks=%u errno=%d error=%s",
                                  pos, blocks, saved_errno, strerror(saved_errno)));
        return make_read_result(SACD_INPUT_RETRIABLE, 0, saved_errno, strerror(saved_errno));
    }
    if (ret == 0)
        return make_read_result(SACD_INPUT_EOF, 0, 0, "end of input");

    if((size_t)ret < len)
    {

        /*       Nothing more to read.  Return all of the whole blocks, if any.
             * Adjust the file position back to the previous block boundary.
            On success, the number of bytes read is returned(zero indicates end of file),
            and the file position is advanced by this number.It is not an error if this number
            is smaller than the number of bytes requested; this may happen for example because fewer bytes are
            actually available right now (maybe because we were close to end-of-
            file, or because we are reading from a pipe, or from a terminal), or
            because read() was interrupted by a signal. */
        return make_read_result(SACD_INPUT_SHORT, ((uint32_t)ret) / SACD_LSN_SIZE,
                                0, "short read");
    }

    // read with succes
    return make_read_result(SACD_INPUT_COMPLETE, blocks, 0, "");

}

static uint32_t sacd_dev_input_read(sacd_input_t dev, uint32_t pos, uint32_t blocks, void *buffer)
{
    return sacd_dev_input_read_ex(dev, pos, blocks, buffer).blocks_read;
}

/**
 * close the SACD device and clean up.
 */
static int sacd_dev_input_close(sacd_input_t dev)
{
    int ret;

    ret = close(dev->fd);

    free(dev);

    return ret;
}

static uint32_t sacd_dev_input_total_sectors(sacd_input_t dev)
{
    if (!dev)
        return 0;

    {
        struct stat file_stat;
        if(fstat(dev->fd, &file_stat) < 0)
            return 0;

        return (uint32_t) (file_stat.st_size / SACD_LSN_SIZE);
    }
}

/**
 * initialize and open a SACD device or file.
 */
static sacd_input_t sacd_net_input_open(const char *target)
{
    ServerRequest request;
    ServerResponse response;
    sacd_input_t dev = 0;
    const char *err = 0;
    t_timeout tm;
    pb_istream_t input;
    pb_ostream_t output;
    uint8_t zero = 0;

    /* Allocate the library structure */
    dev = (sacd_input_t) calloc(sizeof(*dev), 1);
    if (dev == NULL)
    {
        fprintf(stderr, "libsacdread: Could not allocate memory.\n");
        LOG(lm_input, LOG_ERROR, ("ERROR in sacd_net_input_open():libsacdread: Could not allocate memory"));
        return NULL;
    }

    dev->input_buffer = (uint8_t *) malloc(MAX_PROCESSING_BLOCK_SIZE * SACD_LSN_SIZE + 1024);
    if (dev->input_buffer == NULL)
    {
        fprintf(stderr, "libsacdread: Could not allocate memory.\n");
        LOG(lm_input, LOG_ERROR, ("ERROR in sacd_net_input_open():libsacdread: Could not allocate memory"));
        goto error;
    }

    socket_open();

    socket_create((p_socket)&dev->fd, AF_INET, SOCK_STREAM, 0);
    socket_setblocking((p_socket)&dev->fd);

    timeout_markstart(&tm);
    err = inet_tryconnect((p_socket)&dev->fd,
                          substr(target, 0, strchr(target, ':') - target),
                          atoi(strchr(target, ':') + 1), &tm);
    if (err)
    {
        fprintf(stderr, "Failed to connect: %s\n", err);

        LOG(lm_input, LOG_ERROR, ("ERROR in sacd_net_input_open(target=%s); Failed to connect! inet_tryconnect() returns error=(%s)",target,err));
        //LOG(lm_input, LOG_ERROR, ("ERROR in sacd_net_input_open(); address=%s, port=%d",substr(target, 0, strchr(target, ':') - target),atoi(strchr(target, ':') + 1)));

        goto error;
    }
    socket_setblocking((p_socket)&dev->fd);

    input = pb_istream_from_socket((p_socket)&dev->fd);

    output = pb_ostream_from_socket((p_socket)&dev->fd);

    request.type = ServerRequest_Type_DISC_OPEN;request.sector_count=0;request.sector_offset=0;

    if (!pb_encode(&output, ServerRequest_fields, &request))
    {
        fprintf(stderr, "Failed to encode request\n");
        goto error;
    }

    /* We signal the end of request with a 0 tag. */
    pb_write(&output, &zero, 1);

    if (!pb_decode(&input, ServerResponse_fields, &response))
    {
        fprintf(stderr, "Failed to decode response\n");
        goto error;
    }

    if (response.result != 0 || response.type != ServerResponse_Type_DISC_OPENED)
    {
        fprintf(stderr, "Response result non-zero or disc opened\n");
        goto error;
    }

    return dev;

error:

    sacd_input_close(dev);

    return 0;
}

/**
 * close the SACD device and clean up.
 */
static int sacd_net_input_close(sacd_input_t dev)
{
    if (!dev)
    {
        return 0;
    }
    else
    {
        ServerRequest request;
        ServerResponse response;
        pb_istream_t input = pb_istream_from_socket((p_socket)&dev->fd);
        pb_ostream_t output = pb_ostream_from_socket((p_socket)&dev->fd);
        uint8_t zero = 0;

        request.type = ServerRequest_Type_DISC_CLOSE;request.sector_count=0;request.sector_offset=0;
        if (!pb_encode(&output, ServerRequest_fields, &request))
        {
            goto error;
        }

        pb_write(&output, &zero, 1);

        if (!pb_decode(&input, ServerResponse_fields, &response))
        {
            goto error;
        }

        if (response.result == 0 || response.type != ServerResponse_Type_DISC_CLOSED)
        {
            goto error;
        }
    }

error:

    if(dev)
    {
        socket_destroy((p_socket)&dev->fd);
        socket_close();
        if (dev->input_buffer)
        {
            free(dev->input_buffer);
            dev->input_buffer = 0;
        }
        free(dev);
        dev = 0;
    }
    return 0;
}

static uint32_t sacd_net_input_total_sectors(sacd_input_t dev)
{
    if (!dev)
    {
        return 0;
    }
    else
    {
        ServerRequest request;
        ServerResponse response;
        pb_istream_t input = pb_istream_from_socket((p_socket)&dev->fd);
        pb_ostream_t output = pb_ostream_from_socket((p_socket)&dev->fd);
        uint8_t zero = 0;

        request.type = ServerRequest_Type_DISC_SIZE;request.sector_count=0;request.sector_offset=0;

        if (!pb_encode(&output, ServerRequest_fields, &request))
        {
            return 0;
        }

        /* We signal the end of request with a 0 tag. */
        pb_write(&output, &zero, 1);

        if (!pb_decode(&input, ServerResponse_fields, &response))
        {
            return 0;
        }

        if (response.type != ServerResponse_Type_DISC_SIZE)
        {
            return 0;
        }

        return (uint32_t) response.result;
    }
}

static sacd_input_read_result_t sacd_net_input_read_ex(sacd_input_t dev, uint32_t pos,
                                                       uint32_t blocks, void *buffer)
{
    if (!dev)
        return make_read_result(SACD_INPUT_FATAL, 0, EINVAL, "network input is not open");

    uint8_t output_buf[16];
    ServerRequest request;
    ServerResponse response;
    pb_ostream_t output = pb_ostream_from_buffer(output_buf, sizeof(output_buf));
    pb_istream_t input = pb_istream_from_socket((p_socket)&dev->fd);
    uint8_t zero = 0;

    request.type = ServerRequest_Type_DISC_READ;
    request.sector_offset = pos;
    request.sector_count = blocks;

    if (!pb_encode(&output, ServerRequest_fields, &request) || !pb_write(&output, &zero, 1))
        return make_read_result(SACD_INPUT_FATAL, 0, EPROTO,
                                "unable to encode network read request");

    /* A failed send is retryable only if no request bytes reached the stream.
     * A partial request leaves request framing ambiguous and is fatal. */
    {
        size_t written = 0;
        int send_result = socket_send((p_socket)&dev->fd, (char *)output_buf,
                                      output.bytes_written, &written, 0, 0);
        if (send_result != IO_DONE || written != output.bytes_written)
        {
            if (written == 0)
                return make_read_result(SACD_INPUT_RETRIABLE, 0, EIO,
                                        "network request was not sent");
            return make_read_result(SACD_INPUT_FATAL, 0, EPROTO,
                                    "partial network request desynchronized protocol framing");
        }
    }

    response.data.bytes = buffer;
    if (!pb_decode(&input, ServerResponse_fields, &response))
        return make_read_result(SACD_INPUT_FATAL, 0, EPROTO,
                                "network response desynchronized protocol framing");
    if (response.type != ServerResponse_Type_DISC_READ || response.result > blocks)
        return make_read_result(SACD_INPUT_FATAL, 0, EPROTO,
                                "invalid network read response");

    /* A complete error response consumed valid framing, so retrying the next
     * request on this connection is safe. */
    if (!response.has_data)
        return make_read_result(SACD_INPUT_RETRIABLE, 0, EIO,
                                "network peer did not return sector data");
    if (response.result == blocks)
        return make_read_result(SACD_INPUT_COMPLETE, (uint32_t)response.result, 0, "");
    if (response.result > 0)
        return make_read_result(SACD_INPUT_SHORT, (uint32_t)response.result, 0,
                                "short network response");
    return make_read_result(SACD_INPUT_EOF, 0, 0, "network peer returned no sectors");
}

static uint32_t sacd_net_input_read(sacd_input_t dev, uint32_t pos, uint32_t blocks, void *buffer)
{
    return sacd_net_input_read_ex(dev, pos, blocks, buffer).blocks_read;
}

/**
 * Setup read functions with either network or file access
 */
int sacd_input_setup(const char* path)
{
    int net_conn = 0;
    {
        // TODO: replace this F*(&^*($#^(&*#^$GLY hack to detect IP
        int i = 0;
        const char *c = path;
        while ((c = strchr(c + 1, '.')))
        {
            if (++i == 3 && strchr(c + 1, ':'))
            {
                net_conn = 1;
                break;
            }
        }
    }

    if (net_conn)
    {
        sacd_input_open = sacd_net_input_open;
        sacd_input_close = sacd_net_input_close;
        sacd_input_read = sacd_net_input_read;
        sacd_input_read_ex = sacd_net_input_read_ex;
        sacd_input_error = sacd_dev_input_error;
        sacd_input_total_sectors = sacd_net_input_total_sectors;

        return 1;
    }

    sacd_input_open = sacd_dev_input_open;
    sacd_input_close = sacd_dev_input_close;
    sacd_input_read = sacd_dev_input_read;
    sacd_input_read_ex = sacd_dev_input_read_ex;
    sacd_input_error = sacd_dev_input_error;
    sacd_input_total_sectors = sacd_dev_input_total_sectors;

    return 0;
}
