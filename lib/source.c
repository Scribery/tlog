/*
 * Tlog source.
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

#include <assert.h>
#include <string.h>
#include <errno.h>
#include "tlog/rc.h"
#include "tlog/source.h"

bool
tlog_source_is_valid(const struct tlog_source *source)
{
    return source != NULL &&
           tlog_reader_is_valid(source->reader) &&
           source->io_buf != NULL &&
           source->io_size >= TLOG_SOURCE_IO_SIZE_MIN;
}


void
tlog_source_cleanup(struct tlog_source *source)
{
    assert(source != NULL);
    tlog_msg_cleanup(&source->msg);
    free(source->hostname);
    source->hostname = NULL;
    free(source->username);
    source->username = NULL;
    free(source->io_buf);
    source->io_buf = NULL;
}

tlog_grc
tlog_source_init(struct tlog_source *source,
                 struct tlog_reader *reader,
                 const char *hostname,
                 const char *username,
                 unsigned int session_id,
                 size_t io_size)
{
    tlog_grc grc;

    assert(source != NULL);
    assert(tlog_reader_is_valid(reader));
    assert(io_size >= TLOG_SOURCE_IO_SIZE_MIN);

    memset(source, 0, sizeof(*source));
    source->reader = reader;
    if (hostname != NULL) {
        source->hostname = strdup(hostname);
        if (source->hostname == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
    }
    if (username != NULL) {
        source->username = strdup(username);
        if (source->username == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
    }
    source->session_id = session_id;

    tlog_msg_init(&source->msg, NULL);

    source->io_size = io_size;
    source->io_buf = malloc(io_size);
    if (source->io_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    assert(tlog_source_is_valid(source));

    return TLOG_RC_OK;
error:
    tlog_source_cleanup(source);
    return grc;
}

tlog_grc
tlog_source_create(struct tlog_source **psource,
                   struct tlog_reader *reader,
                   const char *hostname,
                   const char *username,
                   unsigned int session_id,
                   size_t io_size)
{
    struct tlog_source *source;
    tlog_grc grc;

    assert(psource != NULL);
    assert(tlog_reader_is_valid(reader));
    assert(io_size >= TLOG_SOURCE_IO_SIZE_MIN);

    source = malloc(sizeof(*source));
    if (source == NULL) {
        grc = TLOG_GRC_ERRNO;
    } else {
        grc = tlog_source_init(source, reader,
                               hostname, username, session_id, io_size);
        if (grc != TLOG_RC_OK) {
            free(source);
            source = NULL;
        }
    }

    *psource = source;
    return grc;
}

void
tlog_source_destroy(struct tlog_source *source)
{
    if (source == NULL)
        return;
    tlog_source_cleanup(source);
    free(source);
}

size_t
tlog_source_loc_get(const struct tlog_source *source)
{
    assert(tlog_source_is_valid(source));
    return tlog_reader_loc_get(source->reader);
}

char *
tlog_source_loc_fmt(const struct tlog_source *source, size_t loc)
{
    assert(tlog_source_is_valid(source));
    return tlog_reader_loc_fmt(source->reader, loc);
}

/**
 * Read a matching JSON message from the source's reader.
 *
 * @param source    The source to read message for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_source_read_msg(struct tlog_source *source)
{
    tlog_grc grc;
    struct json_object *obj;

    assert(tlog_source_is_valid(source));
    assert(tlog_msg_is_void(&source->msg));

    for (; ; tlog_msg_cleanup(&source->msg)) {
        grc = tlog_reader_read(source->reader, &obj);
        if (grc != TLOG_RC_OK)
            return grc;
        if (obj == NULL)
            return TLOG_RC_OK;

        grc = tlog_msg_init(&source->msg, obj);
        json_object_put(obj);
        if (grc != TLOG_RC_OK)
            return grc;

        if (source->hostname != NULL &&
            strcmp(source->msg.host, source->hostname) != 0)
            continue;
        if (source->username != NULL &&
            strcmp(source->msg.user, source->username) != 0)
            continue;
        if (source->session_id != 0 &&
            source->msg.session != source->session_id)
            continue;

        return TLOG_RC_OK;
    }
}

tlog_grc
tlog_source_read(struct tlog_source *source, struct tlog_pkt *pkt)
{
    tlog_grc grc;

    assert(tlog_source_is_valid(source));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));

    while (true) {
        if (tlog_msg_is_void(&source->msg)) {
            grc = tlog_source_read_msg(source);
            if (grc != TLOG_RC_OK)
                return grc;
            if (tlog_msg_is_void(&source->msg))
                return TLOG_RC_OK;
        }

        grc = tlog_msg_read(&source->msg, pkt,
                            source->io_buf, source->io_size);
        if (grc != TLOG_RC_OK) {
            tlog_msg_cleanup(&source->msg);
            return grc;
        }

        if (tlog_pkt_is_void(pkt)) {
            tlog_msg_cleanup(&source->msg);
        } else {
            return TLOG_RC_OK;
        }
    }
}
