/*
 * Tlog sink.
 *
 * Copyright (C) 2015 Red Hat
 *
 * This file is part of tlog.
 *
 * Tlog is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Tlog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tlog; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "tlog_sink.h"
#include "tlog_misc.h"

void
tlog_sink_cleanup(struct tlog_sink *sink)
{
    assert(sink != NULL);
    tlog_io_cleanup(&sink->io);
    free(sink->message_buf);
    sink->message_buf = NULL;
    free(sink->hostname);
    sink->hostname = NULL;
}

tlog_rc
tlog_sink_init(struct tlog_sink *sink,
               int fd,
               const char *hostname,
               unsigned int session_id,
               size_t io_size)
{
    int orig_errno;
    struct timespec res;

    assert(sink != NULL);
    assert(fd >= 0);
    assert(hostname != NULL);
    assert(io_size >= TLOG_IO_SIZE_MIN);

    memset(sink, 0, sizeof(*sink));

    sink->fd = fd;

    sink->hostname = strdup(hostname);
    if (sink->hostname == NULL)
        goto error;

    sink->session_id = session_id;

    sink->message_id = 0;

    /*
     * Try to use coarse monotonic clock (which is faster),
     * if it provides resolution of at least one millisecond.
     */
    if (clock_getres(CLOCK_MONOTONIC_COARSE, &res) == 0 &&
        res.tv_sec == 0 && res.tv_nsec < 1000000)
        sink->clock_id = CLOCK_MONOTONIC_COARSE;
    else if (clock_getres(CLOCK_MONOTONIC, NULL) == 0)
        sink->clock_id = CLOCK_MONOTONIC;
    else
        abort();
    clock_gettime(sink->clock_id, &sink->start);

    /* NOTE: approximate size */
    sink->message_len = io_size + 1024;
    sink->message_buf = malloc(sink->message_len);
    if (sink->message_buf == NULL)
        goto error;

    if (tlog_io_init(&sink->io, io_size) != TLOG_RC_OK)
        goto error;

    return TLOG_RC_OK;

error:
    orig_errno = errno;
    tlog_sink_cleanup(sink);
    errno = orig_errno;
    return TLOG_RC_FAILURE;
}

tlog_rc
tlog_sink_create(struct tlog_sink **psink,
                 int fd,
                 const char *hostname,
                 unsigned int session_id,
                 size_t io_size)
{
    struct tlog_sink *sink;

    assert(fd >= 0);
    assert(hostname != NULL);
    assert(io_size >= TLOG_IO_SIZE_MIN);

    sink = malloc(sizeof(*sink));
    if (sink == NULL)
        return TLOG_RC_FAILURE;

    if (tlog_sink_init(sink, fd, hostname, session_id, io_size) !=
            TLOG_RC_OK) {
        free(sink);
        return TLOG_RC_FAILURE;
    }

    if (psink == NULL)
        tlog_sink_destroy(sink);
    else
        *psink = sink;
    return TLOG_RC_OK;
};

void
tlog_sink_destroy(struct tlog_sink *sink)
{
    if (sink != NULL) {
        tlog_sink_cleanup(sink);
        free(sink);
    }
}

bool
tlog_sink_is_valid(const struct tlog_sink *sink)
{
    return sink != NULL &&
           sink->fd >= 0 &&
           sink->hostname != NULL &&
           tlog_io_is_valid(&sink->io);
}

/**
 * Write to a file descriptor, retrying on incomplete writes and EINTR.
 *
 * @param fd    File descriptor to write to.
 * @param buf   Pointer to the buffer to write.
 * @param len   Length of the buffer to write.
 *
 * @return Status code.
 */
static tlog_rc
tlog_sink_retried_write(int fd, uint8_t *buf, size_t len)
{
    ssize_t rc;
    assert(fd >= 0);
    assert(buf != NULL || len == 0);

    while (true) {
        rc = write(fd, buf, len);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            else
                return TLOG_RC_FAILURE;
        }
        if ((size_t)rc == len)
            return TLOG_RC_OK;
        buf += rc;
        len -= (size_t)rc;
    }
}

tlog_rc
tlog_sink_window_write(struct tlog_sink *sink,
                       unsigned short int width,
                       unsigned short int height)
{
    struct timespec timestamp;
    struct timespec pos;
    int len;

    assert(tlog_sink_is_valid(sink));

    clock_gettime(sink->clock_id, &timestamp);
    tlog_timespec_sub(&timestamp, &sink->start, &pos);

    tlog_sink_io_flush(sink);

    len = snprintf(
        (char *)sink->message_buf, sink->message_len,
        "{"
            "\"type\":"     "\"window\","
            "\"hostname\":" "\"%s\","
            "\"session\":"  "%u,"
            "\"id\":"       "%zu,"
            "\"pos\":"      "%ld.%03ld,"
            "\"width\":"    "%hu,"
            "\"height\":"   "%hu"
        "}\n",
        sink->hostname,
        sink->session_id,
        sink->message_id,
        pos.tv_sec, pos.tv_nsec / 1000000,
        width,
        height);
    if (len < 0)
        return TLOG_RC_FAILURE;
    if ((size_t)len >= sink->message_len) {
        errno = ENOMEM;
        return TLOG_RC_FAILURE;
    }

    sink->message_id++;

    return tlog_sink_retried_write(sink->fd, sink->message_buf, len);
}

tlog_rc
tlog_sink_io_write(struct tlog_sink *sink, bool output,
                   const uint8_t *buf, size_t len)
{
    struct timespec timestamp;

    assert(tlog_sink_is_valid(sink));
    assert(buf != NULL || len == 0);

    clock_gettime(sink->clock_id, &timestamp);

    while (true) {
        tlog_io_write(&sink->io, &timestamp, output, &buf, &len);
        if (len == 0) {
            break;
        } else {
            if (tlog_sink_io_flush(sink) != TLOG_RC_OK)
                return TLOG_RC_FAILURE;
        }
    }
    return TLOG_RC_OK;
}

tlog_rc
tlog_sink_io_cut(struct tlog_sink *sink)
{
    assert(tlog_sink_is_valid(sink));
    while (!tlog_io_cut(&sink->io)) {
        if (tlog_sink_io_flush(sink) != TLOG_RC_OK)
            return TLOG_RC_FAILURE;
    }
    return TLOG_RC_OK;
}

tlog_rc
tlog_sink_io_flush(struct tlog_sink *sink)
{
    int len;
    struct timespec pos;

    assert(tlog_sink_is_valid(sink));

    if (tlog_io_is_empty(&sink->io))
        return TLOG_RC_OK;

    /* Write terminating metadata records to reserved space */
    tlog_io_flush(&sink->io);

    tlog_timespec_sub(&sink->io.first, &sink->start, &pos);
    len = snprintf(
        (char *)sink->message_buf, sink->message_len,
        "{"
            "\"type\":"     "\"io\","
            "\"hostname\":" "\"%s\","
            "\"session\":"  "%u,"
            "\"id\":"       "%zu,"
            "\"pos\":"      "%ld.%03ld,"
            "\"timing\":"   "\"%.*s\","
            "\"in_txt\":"   "\"%.*s\","
            "\"in_bin\":"   "[%.*s],"
            "\"out_txt\":"  "\"%.*s\","
            "\"out_bin\":"  "[%.*s]"
        "}\n",
        sink->hostname,
        sink->session_id,
        sink->message_id,
        pos.tv_sec, pos.tv_nsec / 1000000,
        (int)(sink->io.timing_ptr - sink->io.timing_buf), sink->io.timing_buf,
        (int)sink->io.input.txt_len, sink->io.input.txt_buf,
        (int)sink->io.input.bin_len, sink->io.input.bin_buf,
        (int)sink->io.output.txt_len, sink->io.output.txt_buf,
        (int)sink->io.output.bin_len, sink->io.output.bin_buf);
    if (len < 0)
        return TLOG_RC_FAILURE;
    if ((size_t)len >= sink->message_len) {
        errno = ENOMEM;
        return TLOG_RC_FAILURE;
    }

    if (tlog_sink_retried_write(sink->fd, sink->message_buf, len) !=
            TLOG_RC_OK)
        return TLOG_RC_FAILURE;

    sink->message_id++;
    tlog_io_empty(&sink->io);

    return TLOG_RC_OK;
}
