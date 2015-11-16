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
#include <syslog.h>
#include <tlog/sink.h>
#include <tlog/misc.h>

void
tlog_sink_cleanup(struct tlog_sink *sink)
{
    assert(sink != NULL);
    tlog_chunk_cleanup(&sink->chunk);
    free(sink->message_buf);
    sink->message_buf = NULL;
    free(sink->username);
    sink->username = NULL;
    free(sink->hostname);
    sink->hostname = NULL;
}

tlog_grc
tlog_sink_init(struct tlog_sink *sink,
               struct tlog_writer *writer,
               const char *hostname,
               const char *username,
               unsigned int session_id,
               size_t chunk_size)
{
    tlog_grc grc;

    assert(sink != NULL);
    assert(tlog_writer_is_valid(writer));
    assert(hostname != NULL);
    assert(chunk_size >= TLOG_SINK_CHUNK_SIZE_MIN);

    memset(sink, 0, sizeof(*sink));

    sink->writer = writer;

    sink->hostname = strdup(hostname);
    if (sink->hostname == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    sink->username = strdup(username);
    if (sink->username == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    sink->session_id = session_id;

    sink->message_id = 1;

    /* NOTE: approximate size */
    sink->message_len = chunk_size + 1024;
    sink->message_buf = malloc(sink->message_len);
    if (sink->message_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    grc = tlog_chunk_init(&sink->chunk, chunk_size);
    if (grc != TLOG_RC_OK)
        goto error;

    assert(tlog_sink_is_valid(sink));
    return TLOG_RC_OK;

error:
    tlog_sink_cleanup(sink);
    return grc;
}

tlog_grc
tlog_sink_create(struct tlog_sink **psink,
                 struct tlog_writer *writer,
                 const char *hostname,
                 const char *username,
                 unsigned int session_id,
                 size_t chunk_size)
{
    struct tlog_sink *sink;
    tlog_grc grc;

    assert(psink != NULL);
    assert(tlog_writer_is_valid(writer));
    assert(hostname != NULL);
    assert(chunk_size >= TLOG_SINK_CHUNK_SIZE_MIN);

    sink = malloc(sizeof(*sink));
    if (sink == NULL) {
        grc = TLOG_GRC_ERRNO;
    } else {
        grc = tlog_sink_init(sink, writer, hostname, username,
                             session_id, chunk_size);
        if (grc == TLOG_RC_OK) {
            assert(tlog_sink_is_valid(sink));
        } else {
            free(sink);
            sink = NULL;
        }
    }

    *psink = sink;
    return grc;
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
           tlog_writer_is_valid(sink->writer) &&
           sink->hostname != NULL &&
           sink->username != NULL &&
           sink->message_buf != NULL &&
           tlog_chunk_is_valid(&sink->chunk);
}

tlog_grc
tlog_sink_write(struct tlog_sink *sink, const struct tlog_pkt *pkt)
{
    tlog_grc grc;
    size_t pos = 0;

    assert(tlog_sink_is_valid(sink));
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));

    if (!sink->started) {
        sink->started = true;
        sink->start = pkt->timestamp;
    }

    /* While the packet is not yet written completely */
    while (!tlog_chunk_write(&sink->chunk, pkt, &pos)) {
        grc = tlog_sink_flush(sink);
        if (grc != TLOG_RC_OK)
            return grc;
    }
    return TLOG_RC_OK;
}

tlog_grc
tlog_sink_cut(struct tlog_sink *sink)
{
    tlog_grc grc;
    assert(tlog_sink_is_valid(sink));
    while (!tlog_chunk_cut(&sink->chunk)) {
        grc = tlog_sink_flush(sink);
        if (grc != TLOG_RC_OK)
            return grc;
    }
    return TLOG_RC_OK;
}

tlog_grc
tlog_sink_flush(struct tlog_sink *sink)
{
    tlog_grc grc;
    char pos_buf[32];
    int len;
    struct timespec pos;

    assert(tlog_sink_is_valid(sink));

    if (tlog_chunk_is_empty(&sink->chunk))
        return TLOG_RC_OK;

    /* Write terminating metadata records to reserved space */
    tlog_chunk_flush(&sink->chunk);

    tlog_timespec_sub(&sink->chunk.first, &sink->start, &pos);

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
        (char *)sink->message_buf, sink->message_len,
        "{"
            "\"host\":"     "\"%s\","
            "\"user\":"     "\"%s\","
            "\"session\":"  "%u,"
            "\"id\":"       "%zu,"
            "\"pos\":"      "%s,"
            "\"timing\":"   "\"%.*s\","
            "\"in_txt\":"   "\"%.*s\","
            "\"in_bin\":"   "[%.*s],"
            "\"out_txt\":"  "\"%.*s\","
            "\"out_bin\":"  "[%.*s]"
        "}\n",
        sink->hostname,
        sink->username,
        sink->session_id,
        sink->message_id,
        pos_buf,
        (int)(sink->chunk.timing_ptr - sink->chunk.timing_buf),
        sink->chunk.timing_buf,
        (int)sink->chunk.input.txt_len, sink->chunk.input.txt_buf,
        (int)sink->chunk.input.bin_len, sink->chunk.input.bin_buf,
        (int)sink->chunk.output.txt_len, sink->chunk.output.txt_buf,
        (int)sink->chunk.output.bin_len, sink->chunk.output.bin_buf);
    if (len < 0)
        return TLOG_RC_FAILURE;
    if ((size_t)len >= sink->message_len) {
        return TLOG_GRC_FROM(errno, ENOMEM);
    }

    grc = tlog_writer_write(sink->writer, sink->message_buf, len);
    if (grc != TLOG_RC_OK)
        return grc;

    sink->message_id++;
    tlog_chunk_empty(&sink->chunk);

    return TLOG_RC_OK;
}
