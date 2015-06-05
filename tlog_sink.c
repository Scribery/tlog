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

static void
tlog_sink_io_cleanup(struct tlog_sink_io *io)
{
    assert(io != NULL);
    free(io->input_buf);
    io->input_buf = NULL;
    free(io->output_buf);
    io->output_buf = NULL;
    free(io->timing_buf);
    io->timing_buf = NULL;
}

void
tlog_sink_cleanup(struct tlog_sink *sink)
{
    assert(sink != NULL);
    tlog_sink_io_cleanup(&sink->io);
    free(sink->message_buf);
    sink->message_buf = NULL;
    free(sink->hostname);
    sink->hostname = NULL;
}

static tlog_rc
tlog_sink_io_init(struct tlog_sink_io *io,
                  size_t max_len)
{
    int orig_errno;

    assert(io != NULL);
    assert(max_len >= TLOG_SINK_IO_MAX_LEN_MIN);

    memset(io, 0, sizeof(*io));

    io->max_len = max_len;

    io->input_buf = malloc(max_len);
    if (io->input_buf == NULL)
        goto error;
    io->output_buf = malloc(max_len);
    if (io->output_buf == NULL)
        goto error;
    io->timing_buf = malloc(max_len);
    if (io->timing_buf == NULL)
        goto error;

    return TLOG_RC_OK;

error:
    orig_errno = errno;
    tlog_sink_io_cleanup(io);
    errno = orig_errno;
    return TLOG_RC_FAILURE;
}

tlog_rc
tlog_sink_init(struct tlog_sink *sink,
               int fd,
               const char *hostname,
               unsigned int session_id,
               size_t io_max_len)
{
    int orig_errno;

    assert(sink != NULL);
    assert(fd >= 0);
    assert(hostname != NULL);
    assert(io_max_len >= TLOG_SINK_IO_MAX_LEN_MIN);

    memset(sink, 0, sizeof(*sink));

    sink->fd = fd;

    sink->hostname = strdup(hostname);
    if (sink->hostname == NULL)
        goto error;

    sink->session_id = session_id;

    sink->message_id = 0;

    /*
     * Try to use coarse monotonic clock which is faster and
     * is good enough for us.
     */
    if (clock_getres(CLOCK_MONOTONIC_COARSE, NULL) == 0)
        sink->clock_id = CLOCK_MONOTONIC_COARSE;
    else if (clock_getres(CLOCK_MONOTONIC, NULL) == 0)
        sink->clock_id = CLOCK_MONOTONIC;
    else
        abort();
    clock_gettime(sink->clock_id, &sink->start);

    /* NOTE: approximate size */
    sink->message_len = io_max_len + 1024;
    sink->message_buf = malloc(sink->message_len);
    if (sink->message_buf == NULL)
        goto error;

    if (tlog_sink_io_init(&sink->io, io_max_len) != TLOG_RC_OK)
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
                 size_t io_max_len)
{
    struct tlog_sink *sink;

    assert(fd >= 0);
    assert(hostname != NULL);
    assert(io_max_len >= TLOG_SINK_IO_MAX_LEN_MIN);

    sink = malloc(sizeof(*sink));
    if (sink == NULL)
        return TLOG_RC_FAILURE;

    if (tlog_sink_init(sink, fd, hostname, session_id, io_max_len) !=
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

static bool
tlog_sink_io_valid(const struct tlog_sink_io *io)
{
    return io != NULL &&
           io->max_len > 0 &&
           io->input_buf != NULL &&
           io->input_len <= io->max_len &&
           io->output_buf != NULL &&
           io->output_len <= io->max_len &&
           io->timing_buf != NULL &&
           io->timing_len <= io->max_len &&
           (io->input_len + io->output_len + io->timing_len) == io->len;
}

bool
tlog_sink_valid(const struct tlog_sink *sink)
{
    return sink != NULL &&
           sink->fd >= 0 &&
           sink->hostname != NULL &&
           tlog_sink_io_valid(&sink->io);
}

tlog_rc
tlog_sink_write_window(struct tlog_sink *sink,
                       unsigned short int width,
                       unsigned short int height)
{
    struct timespec timestamp;
    struct timespec pos;
    int len;

    assert(tlog_sink_valid(sink));

    clock_gettime(sink->clock_id, &timestamp);
    tlog_timespec_sub(&timestamp, &sink->start, &pos);

    tlog_sink_flush(sink);

    len = snprintf(
        (char *)sink->message_buf, sink->message_len,
        "{"
            "\"type\":"     "\"window\","
            "\"hostname\":" "\"%s\","
            "\"session\":"  "%u,"
            "\"id\":"       "%zu,"
            "\"pos\":"      "%ld.%06ld,"
            "\"width\":"    "%hu,"
            "\"height\":"   "%hu"
        "}\n",
        sink->hostname,
        sink->session_id,
        sink->message_id,
        pos.tv_sec, pos.tv_nsec / 1000,
        width,
        height);
    if (len < 0)
        return TLOG_RC_FAILURE;
    if ((size_t)len >= sink->message_len) {
        errno = ENOMEM;
        return TLOG_RC_FAILURE;
    }

    sink->message_id++;

    if (write(sink->fd, sink->message_buf, len) < len)
        return TLOG_RC_FAILURE;

    return TLOG_RC_OK;
}

/**
 * Encode (a part of) an I/O buffer into a JSON string buffer,
 * reserving space for adding output length in decimal.
 *
 * @param obuf  Output buffer pointer.
 * @param olen  Output buffer length.
 * @param ponum Location for the number of bytes output, can be NULL.
 * @param ibuf  Input buffer pointer.
 * @param ilen  Input buffer length.
 * @param pinum Location for the number of bytes input, can be NULL.
 */
static void
tlog_sink_encode(uint8_t *obuf, size_t olen, size_t *ponum,
                 const uint8_t *ibuf, size_t ilen, size_t *pinum)
{
    uint8_t c;
    uint8_t *optr = obuf;
    const uint8_t *iptr = ibuf;
    size_t next_onum = 0;
    size_t digit_limit = 10;

    /* Account for the single-digit zero length output */
    if (olen <= 1)
        goto finish;
    olen--;

#define OREQ(_l) \
    do {                                    \
        next_onum += _l;                    \
        /* If we'll need one more digit */  \
        /* NOTE: Assuming _l < 100 */       \
        if (next_onum >= digit_limit) {     \
            if ((_l) >= olen) {             \
                goto finish;                \
            } else {                        \
                olen -= (_l) + 1;           \
                digit_limit *= 10;          \
            }                               \
        } else if ((_l) > olen) {           \
            goto finish;                    \
        } else {                            \
            olen -= _l;                     \
        }                                   \
    } while (0);

    for (; ilen > 0; iptr++, ilen--) {
        c = *iptr;
        switch (c) {
            case '"':
            case '\\':
            case '/':
                OREQ(2);
                *optr++ = '\\';
                *optr++ = c;
                break;
#define ESC_CASE(_c, _e) \
            case _c:            \
                OREQ(2);        \
                *optr++ = '\\'; \
                *optr++ = _e;   \
                break;
            ESC_CASE('\b', 'b');
            ESC_CASE('\f', 'f');
            ESC_CASE('\n', 'n');
            ESC_CASE('\r', 'r');
            ESC_CASE('\t', 't');
#undef ESC
            default:
                if (c < 0x20 || c == 0x7f) {
                    OREQ(6);
                    *optr++ = '\\';
                    *optr++ = 'u';
                    *optr++ = '0';
                    *optr++ = '0';
                    *optr++ = tlog_nibble_digit(c >> 4);
                    *optr++ = tlog_nibble_digit(c & 0xf);
                    break;
                } else if (c >= 0x80) {
                    OREQ(2);
                    *optr++ = 0xc0 | (c >> 6);
                    *optr++ = c & 0x3f;
                } else {
                    OREQ(1);
                    *optr++ = c;
                }
                break;
        }
    }

#undef OREQ

finish:
    if (ponum != NULL)
        *ponum = optr - obuf;
    if (pinum != NULL)
        *pinum = iptr - ibuf;
}

/**
 * Format a timing buffer record: optional delay, plus I/O amount.
 *
 * @param buf       Output buffer pointer.
 * @param len       Output buffer length.
 * @param io_type   I/O type character.
 * @param io_len    I/O length.
 *
 * @return Number of characters (to) output.
 */
static size_t
tlog_sink_format_timing(uint8_t *buf, size_t len,
                        struct timespec *delay,
                        uint8_t io_type,
                        size_t io_len)
{
    size_t olen = 0;
    long sec;
    long usec;
    int rc;

    assert(buf != NULL || len == 0);
    assert(delay != NULL);

    sec = (long)delay->tv_sec;
    usec = delay->tv_nsec / 1000;

    if (sec == 0) {
        if (usec == 0)
            rc = 0;
        else
            rc = snprintf((char *)buf, len, "+%ld", usec);
    } else {
        rc = snprintf((char *)buf, len, "+%ld%06ld", sec, usec);
    }

    assert(rc >= 0);
    olen += (size_t)rc;
    if (olen <= len) {
        len -= olen;
        buf += olen;
    } else {
        len = 0;
    }

    if (io_len == 0)
        rc = 0;
    else
        rc = snprintf((char *)buf, len, "%c%zu", io_type, io_len);

    assert(rc >= 0);
    olen += (size_t)rc;

    return olen;
}
                        
/**
 * Write terminal I/O to a log sink.
 *
 * @param sink      Pointer to the sink to write I/O to.
 * @param type      I/O type character.
 * @param obuf      Output data buffer pointer.
 * @param polen     Location of/for output data length.
 * @param ibuf      Data buffer pointer.
 * @param ilen      Data buffer length.
 *
 * @return Status code.
 */
static tlog_rc
tlog_sink_write_io(struct tlog_sink *sink, uint8_t type,
                   uint8_t *obuf, size_t *polen,
                   const uint8_t *ibuf, size_t ilen)
{
    struct timespec timestamp;
    struct timespec delay;
    size_t delay_len;
    size_t inum;
    size_t onum;

    assert(tlog_sink_valid(sink));
    assert(obuf != NULL);
    assert(polen != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0)
        return TLOG_RC_OK;

    /* Get current timestamp */
    clock_gettime(sink->clock_id, &timestamp);
    /* If this is the first write for this message */
    if (tlog_timespec_is_zero(&sink->io.first)) {
        sink->io.first = timestamp;
        sink->io.last = timestamp;
        tlog_timespec_zero(&delay);
    } else {
        tlog_timespec_sub(&timestamp, &sink->io.last, &delay);
        sink->io.last = timestamp;
    }

    while (ilen > 0) {
        /* Calculate delay data length */
        delay_len = tlog_sink_format_timing(NULL, 0, &delay, 0, 0);
        /*
         * If the delay data and minimal data length overhead
         * is too big for the remaining space.
         */
        if ((sink->io.len + (delay_len + 2) * 2) >= sink->io.max_len) {
            if (tlog_sink_flush(sink) != TLOG_RC_OK)
                return TLOG_RC_FAILURE;
            continue;
        };
        /* Append encoded data, reserving space for timing */
        /* NOTE: tlog_sink_encode accounts for space to store data length */
        tlog_sink_encode(obuf + *polen,
                         /* Account for delay data length plus I/O type */
                         sink->io.max_len - (sink->io.len + delay_len + 1),
                         &onum,
                         ibuf, ilen, &inum);
        /* If no data did fit */
        if (onum == 0) {
            if (tlog_sink_flush(sink) != TLOG_RC_OK)
                return TLOG_RC_FAILURE;
            continue;
        }
        *polen += onum;
        sink->io.len += onum;
        ibuf += inum;
        ilen -= inum;
        /* Append timing */
        onum = tlog_sink_format_timing(
                    sink->io.timing_buf + sink->io.timing_len,
                    sink->io.max_len - sink->io.len,
                    &delay, type, onum);
        sink->io.timing_len += onum;
        sink->io.len += onum;
        /* We must calculate this correctly above */
        assert(sink->io.len <= sink->io.max_len);
    }

    assert(tlog_sink_valid(sink));

    return TLOG_RC_OK;
}

tlog_rc
tlog_sink_write_input(struct tlog_sink *sink,
                      const uint8_t *buf, size_t len)
{
    assert(tlog_sink_valid(sink));
    assert(buf != NULL || len == 0);
    return tlog_sink_write_io(sink, '<',
                              sink->io.input_buf, &sink->io.input_len,
                              buf, len);
}

tlog_rc
tlog_sink_write_output(struct tlog_sink *sink,
                       const uint8_t *buf, size_t len)
{
    assert(tlog_sink_valid(sink));
    assert(buf != NULL || len == 0);
    return tlog_sink_write_io(sink, '>',
                              sink->io.output_buf, &sink->io.output_len,
                              buf, len);
}

tlog_rc
tlog_sink_flush(struct tlog_sink *sink)
{
    int len;
    struct timespec pos;

    assert(tlog_sink_valid(sink));

    if (sink->io.len == 0)
        return TLOG_RC_OK;

    tlog_timespec_sub(&sink->io.first, &sink->start, &pos);
    len = snprintf(
        (char *)sink->message_buf, sink->message_len,
        "{"
            "\"type\":"     "\"io\","
            "\"hostname\":" "\"%s\","
            "\"session\":"  "%u,"
            "\"id\":"       "%zu,"
            "\"pos\":"      "%ld.%06ld,"
            "\"timing\":"   "\"%.*s\","
            "\"input\":"    "\"%.*s\","
            "\"output\":"   "\"%.*s\""
        "}\n",
        sink->hostname,
        sink->session_id,
        sink->message_id,
        pos.tv_sec, pos.tv_nsec / 1000,
        (int)sink->io.timing_len, sink->io.timing_buf,
        (int)sink->io.input_len, sink->io.input_buf,
        (int)sink->io.output_len, sink->io.output_buf);
    if (len < 0)
        return TLOG_RC_FAILURE;
    if ((size_t)len >= sink->message_len) {
        errno = ENOMEM;
        return TLOG_RC_FAILURE;
    }

    sink->message_id++;

    if (write(sink->fd, sink->message_buf, len) < len)
        return TLOG_RC_FAILURE;

    sink->io.timing_len = 0;
    sink->io.input_len = 0;
    sink->io.output_len = 0;
    sink->io.len = 0;
    sink->io.first = TLOG_TIMESPEC_ZERO;
    sink->io.last = TLOG_TIMESPEC_ZERO;

    return TLOG_RC_OK;
}
