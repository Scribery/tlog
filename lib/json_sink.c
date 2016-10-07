/*
 * JSON sink.
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
#include <syslog.h>
#include <tlog/json_sink.h>
#include <tlog/json_misc.h>
#include <tlog/timespec.h>
#include <tlog/delay.h>
#include <tlog/misc.h>

/** JSON sink instance */
struct tlog_json_sink {
    struct tlog_sink            sink;           /**< Abstract sink instance */
    struct tlog_json_writer    *writer;         /**< Log message writer */
    bool                        writer_owned;   /**< True if writer is owned */
    char                       *hostname;       /**< Hostname, JSON-escaped */
    char                       *username;       /**< Username, JSON-escaped */
    char                       *terminal;       /**< Terminal type,
                                                     JSON-escaped */
    unsigned int                session_id;     /**< Session ID */
    size_t                      message_id;     /**< Next message ID */
    bool                        started;        /**< True if a packet
                                                     was written */
    struct timespec             start;          /**< First packet timestamp */
    struct tlog_json_chunk      chunk;          /**< Chunk buffer */
    uint8_t                    *message_buf;    /**< Message buffer pointer */
    size_t                      message_len;    /**< Message buffer length */
};

static void
tlog_json_sink_cleanup(struct tlog_sink *sink)
{
    struct tlog_json_sink *json_sink = (struct tlog_json_sink *)sink;
    assert(json_sink != NULL);
    tlog_json_chunk_cleanup(&json_sink->chunk);
    free(json_sink->message_buf);
    json_sink->message_buf = NULL;
    free(json_sink->terminal);
    json_sink->terminal = NULL;
    free(json_sink->username);
    json_sink->username = NULL;
    free(json_sink->hostname);
    json_sink->hostname = NULL;
    if (json_sink->writer_owned) {
        tlog_json_writer_destroy(json_sink->writer);
        json_sink->writer_owned = false;
    }
}

static tlog_grc
tlog_json_sink_init(struct tlog_sink *sink, va_list ap)
{
    struct tlog_json_sink *json_sink = (struct tlog_json_sink *)sink;
    struct tlog_json_writer *writer = va_arg(ap, struct tlog_json_writer *);
    bool writer_owned = (bool)va_arg(ap, int);
    const char *hostname = va_arg(ap, const char *);
    const char *username = va_arg(ap, const char *);
    const char *terminal = va_arg(ap, const char *);
    unsigned int session_id = va_arg(ap, unsigned int);
    size_t chunk_size = va_arg(ap, size_t);
    tlog_grc grc;

    assert(json_sink != NULL);
    assert(tlog_json_writer_is_valid(writer));
    assert(hostname != NULL);
    assert(tlog_utf8_str_is_valid(hostname));
    assert(username != NULL);
    assert(tlog_utf8_str_is_valid(username));
    assert(terminal != NULL);
    assert(tlog_utf8_str_is_valid(terminal));
    assert(session_id != 0);
    assert(chunk_size >= TLOG_JSON_SINK_CHUNK_SIZE_MIN);

    json_sink->hostname = tlog_json_aesc_str(hostname);
    if (json_sink->hostname == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    json_sink->username = tlog_json_aesc_str(username);
    if (json_sink->username == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    json_sink->terminal = tlog_json_aesc_str(terminal);
    if (json_sink->terminal == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    json_sink->session_id = session_id;

    json_sink->message_id = 1;

    /* NOTE: approximate size */
    json_sink->message_len = chunk_size + 1024;
    json_sink->message_buf = malloc(json_sink->message_len);
    if (json_sink->message_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    grc = tlog_json_chunk_init(&json_sink->chunk, chunk_size);
    if (grc != TLOG_RC_OK) {
        goto error;
    }

    json_sink->writer = writer;
    json_sink->writer_owned = writer_owned;

    return TLOG_RC_OK;

error:
    tlog_json_sink_cleanup(sink);
    return grc;
}

static bool
tlog_json_sink_is_valid(const struct tlog_sink *sink)
{
    struct tlog_json_sink *json_sink = (struct tlog_json_sink *)sink;
    return json_sink != NULL &&
           tlog_json_writer_is_valid(json_sink->writer) &&
           json_sink->hostname != NULL &&
           json_sink->username != NULL &&
           json_sink->terminal != NULL &&
           json_sink->message_buf != NULL &&
           tlog_json_chunk_is_valid(&json_sink->chunk);
}

static tlog_grc
tlog_json_sink_flush(struct tlog_sink *sink)
{
    struct tlog_json_sink *json_sink = (struct tlog_json_sink *)sink;
    tlog_grc grc;
    char pos_buf[32];
    int len;
    struct timespec pos;

    if (tlog_json_chunk_is_empty(&json_sink->chunk)) {
        return TLOG_RC_OK;
    }

    /* Write terminating metadata records to reserved space */
    tlog_json_chunk_flush(&json_sink->chunk);

    tlog_timespec_sub(&json_sink->chunk.first_ts, &json_sink->start, &pos);

    if (pos.tv_sec == 0) {
        len = snprintf(pos_buf, sizeof(pos_buf), "%ld",
                       pos.tv_nsec / 1000000);
    } else {
        len = snprintf(pos_buf, sizeof(pos_buf), "%lld%03ld",
                       (long long int)pos.tv_sec, pos.tv_nsec / 1000000);
    }

    if ((size_t)len >= sizeof(pos_buf)) {
        return TLOG_GRC_FROM(errno, ENOMEM);
    }

    len = snprintf(
        (char *)json_sink->message_buf, json_sink->message_len,
        "{"
            "\"ver\":"      "1,"
            "\"host\":"     "\"%s\","
            "\"user\":"     "\"%s\","
            "\"term\":"     "\"%s\","
            "\"session\":"  "%u,"
            "\"id\":"       "%zu,"
            "\"pos\":"      "%s,"
            "\"timing\":"   "\"%.*s\","
            "\"in_txt\":"   "\"%.*s\","
            "\"in_bin\":"   "[%.*s],"
            "\"out_txt\":"  "\"%.*s\","
            "\"out_bin\":"  "[%.*s]"
        "}\n",
        json_sink->hostname,
        json_sink->username,
        json_sink->terminal,
        json_sink->session_id,
        json_sink->message_id,
        pos_buf,
        (int)(json_sink->chunk.timing_ptr - json_sink->chunk.timing_buf),
        json_sink->chunk.timing_buf,
        (int)json_sink->chunk.input.txt_len, json_sink->chunk.input.txt_buf,
        (int)json_sink->chunk.input.bin_len, json_sink->chunk.input.bin_buf,
        (int)json_sink->chunk.output.txt_len, json_sink->chunk.output.txt_buf,
        (int)json_sink->chunk.output.bin_len, json_sink->chunk.output.bin_buf);
    if (len < 0) {
        return TLOG_RC_FAILURE;
    }
    if ((size_t)len >= json_sink->message_len) {
        return TLOG_GRC_FROM(errno, ENOMEM);
    }

    grc = tlog_json_writer_write(json_sink->writer,
                                 json_sink->message_buf, len);
    if (grc != TLOG_RC_OK) {
        return grc;
    }

    json_sink->message_id++;
    tlog_json_chunk_empty(&json_sink->chunk);

    return TLOG_RC_OK;
}

static tlog_grc
tlog_json_sink_cut(struct tlog_sink *sink)
{
    struct tlog_json_sink *json_sink = (struct tlog_json_sink *)sink;
    tlog_grc grc;

    while (!tlog_json_chunk_cut(&json_sink->chunk)) {
        grc = tlog_json_sink_flush(sink);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
    }

    return TLOG_RC_OK;
}

static tlog_grc
tlog_json_sink_write(struct tlog_sink *sink,
                     const struct tlog_pkt *pkt,
                     struct tlog_pkt_pos *ppos,
                     const struct tlog_pkt_pos *end)
{
    struct tlog_json_sink *json_sink = (struct tlog_json_sink *)sink;
    tlog_grc grc;

    assert(!tlog_pkt_is_void(pkt));

    if (json_sink->started) {
#ifndef NDEBUG
        struct timespec diff;
        tlog_timespec_sub(&pkt->timestamp, &json_sink->start, &diff);
        assert(tlog_timespec_cmp(&diff, &tlog_timespec_zero) >= 0);
        assert(tlog_timespec_cmp(&diff, &tlog_delay_max_timespec) <= 0);
#endif
    } else {
        json_sink->started = true;
        json_sink->start = pkt->timestamp;
    }

    /* While the packet is not yet written completely */
    while (!tlog_json_chunk_write(&json_sink->chunk, pkt, ppos, end)) {
        grc = tlog_json_sink_flush(sink);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
    }
    return TLOG_RC_OK;
}

const struct tlog_sink_type tlog_json_sink_type = {
    .size       = sizeof(struct tlog_json_sink),
    .init       = tlog_json_sink_init,
    .cleanup    = tlog_json_sink_cleanup,
    .is_valid   = tlog_json_sink_is_valid,
    .write      = tlog_json_sink_write,
    .cut        = tlog_json_sink_cut,
    .flush      = tlog_json_sink_flush,
};
