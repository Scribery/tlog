/*
 * Tlog I/O buffer.
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
#include <stdio.h>
#include "tlog_io.h"

tlog_rc
tlog_io_init(struct tlog_io *io, size_t size)
{
    int orig_errno;

    assert(io != NULL);
    assert(size >= TLOG_IO_SIZE_MIN);

    memset(io, 0, sizeof(*io));

    io->size = size;
    io->rem = size;

    if (tlog_stream_init(&io->input, size, '<', '[') != TLOG_RC_OK)
        goto error;
    if (tlog_stream_init(&io->output, size, '>', ']') != TLOG_RC_OK)
        goto error;
    io->timing_buf = malloc(size);
    if (io->timing_buf == NULL)
        goto error;
    io->timing_ptr = io->timing_buf;

    return TLOG_RC_OK;

error:
    orig_errno = errno;
    tlog_io_cleanup(io);
    errno = orig_errno;
    return TLOG_RC_FAILURE;
}

bool
tlog_io_is_valid(const struct tlog_io *io)
{
    return io != NULL &&
           io->size >= TLOG_IO_SIZE_MIN &&
           tlog_stream_is_valid(&io->input) &&
           tlog_stream_is_valid(&io->output) &&
           io->timing_buf != NULL &&
           io->rem <= io->size &&
           ((io->timing_ptr - io->timing_buf) +
            io->input.txt_len + io->input.bin_len +
            io->output.txt_len + io->output.bin_len) <= (io->size - io->rem);
}

bool
tlog_io_is_empty(const struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    return tlog_stream_is_empty(&io->input) &&
           tlog_stream_is_empty(&io->output);
}

size_t
tlog_io_write(struct tlog_io *io, const struct timespec *timestamp,
              bool output, const uint8_t **pbuf, size_t *plen)
{
    struct timespec delay;
    long sec;
    long msec;
    char delay_buf[32];
    int delay_rc;

    uint8_t *new_timing_ptr;
    size_t new_rem;
    struct tlog_stream new_input;
    struct tlog_stream new_output;

    size_t written;

    assert(tlog_io_is_valid(io));
    assert(timestamp != 0);
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);

    if (*plen == 0)
        return 0;

    /*
     * Take a snapshot.
     * We assume this is equivalent to a proper copy for our purposes.
     */
    new_input = io->input;
    new_output = io->output;
    new_timing_ptr = io->timing_ptr;
    new_rem = io->rem;

    /* If this is the first write */
    if (tlog_timespec_is_zero(&io->first)) {
        delay_rc = 0;
    } else {
        tlog_timespec_sub(timestamp, &io->last, &delay);
        sec = (long)delay.tv_sec;
        msec = delay.tv_nsec / 1000000;
        if (sec != 0) {
            delay_rc = snprintf(delay_buf, sizeof(delay_buf),
                                "+%ld%03ld", sec, msec);
        } else if (msec != 0) {
            delay_rc = snprintf(delay_buf, sizeof(delay_buf),
                                "+%ld", msec);
        } else {
            delay_rc = 0;
        }
    }
    assert(delay_rc >= 0);
    assert((size_t)delay_rc < sizeof(delay_buf));

    /* If we need to add a delay record */
    if (delay_rc > 0) {
        /* If it doesn't fit */
        if ((size_t)delay_rc > new_rem)
            return 0;
        tlog_stream_flush(&new_input, &new_timing_ptr);
        tlog_stream_flush(&new_output, &new_timing_ptr);
        memcpy(new_timing_ptr, delay_buf, (size_t)delay_rc);
        new_timing_ptr += delay_rc;
        new_rem -= delay_rc;
    }

    /* Write as much I/O data as we can */
    written = tlog_stream_write(output ? &new_output : &new_input,
                                pbuf, plen, &new_timing_ptr, &new_rem);
    /* If no I/O data fits */
    if (written == 0)
        return 0;

    /*
     * Commit our changes.
     * We assume this is equivalent to a proper copy for our purposes.
     */
    /* If this is the first write */
    if (tlog_timespec_is_zero(&io->first))
        io->first = *timestamp;
    io->last = *timestamp;
    io->input = new_input;
    io->output = new_output;
    io->timing_ptr = new_timing_ptr;
    io->rem = new_rem;
    return written;
}

void
tlog_io_flush(struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    tlog_stream_flush(&io->input, &io->timing_ptr);
    tlog_stream_flush(&io->output, &io->timing_ptr);
}

bool
tlog_io_cut(struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    return tlog_stream_cut(&io->input, &io->timing_ptr, &io->rem) &&
           tlog_stream_cut(&io->output, &io->timing_ptr, &io->rem);
}

void
tlog_io_empty(struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    io->rem = io->size;
    io->timing_ptr = io->timing_buf;
    tlog_stream_empty(&io->input);
    tlog_stream_empty(&io->output);
    tlog_timespec_zero(&io->first);
    tlog_timespec_zero(&io->last);
    assert(tlog_io_is_valid(io));
}

void
tlog_io_cleanup(struct tlog_io *io)
{
    assert(io != NULL);
    tlog_stream_cleanup(&io->input);
    tlog_stream_cleanup(&io->output);
    free(io->timing_buf);
    io->timing_buf = NULL;
}
