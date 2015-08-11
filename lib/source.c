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
    free(source->hostname);
    source->hostname = NULL;
    free(source->username);
    source->username = NULL;
    free(source->io_buf);
    source->io_buf = NULL;
}

int
tlog_source_init(struct tlog_source *source,
                 struct tlog_reader *reader,
                 const char *hostname,
                 const char *username,
                 unsigned int session_id,
                 size_t io_size)
{
    int orig_errno;

    assert(source != NULL);
    assert(tlog_reader_is_valid(reader));
    assert(io_size >= TLOG_SOURCE_IO_SIZE_MIN);

    memset(source, 0, sizeof(*source));
    source->reader = reader;
    source->hostname = hostname ? strdup(hostname) : NULL;
    if (source->hostname == NULL)
        goto error;
    source->username = username ? strdup(username) : NULL;
    if (source->username == NULL)
        goto error;
    source->session_id = session_id;

    tlog_msg_init(&source->msg, NULL);

    source->io_size = io_size;
    source->io_buf = malloc(io_size);
    if (source->io_buf == NULL)
        goto error;

    return 0;
error:
    orig_errno = errno;
    tlog_source_cleanup(source);
    return -orig_errno;
}

int
tlog_source_create(struct tlog_source **psource,
                   struct tlog_reader *reader,
                   const char *hostname,
                   const char *username,
                   unsigned int session_id,
                   size_t io_size)
{
    struct tlog_source *source;
    int rc;

    assert(psource != NULL);
    assert(tlog_reader_is_valid(reader));
    assert(io_size >= TLOG_SOURCE_IO_SIZE_MIN);

    source = malloc(sizeof(*source));
    if (source == NULL)
        return -errno;

    rc = tlog_source_init(source, reader,
                          hostname, username, session_id, io_size);
    if (rc != 0) {
        free(source);
        source = NULL;
    }

    *psource = source;

    return rc;
}

void
tlog_source_destroy(struct tlog_source *source)
{
    if (source == NULL)
        return;
    tlog_source_cleanup(source);
    free(source);
}

const char *
tlog_source_strerror(struct tlog_source *source, int rc)
{
    assert(tlog_source_is_valid(source));

    if (rc >= TLOG_SOURCE_ERROR_MIN) {
        switch (rc) {
        case TLOG_SOURCE_ERROR_INVALID_OBJECT:
            return "Object with invalid schema encountered";
        default:
            return "Unknown error";
        }
    } else {
        return tlog_reader_strerror(source->reader, rc);
    }
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
 * @return Status code.
 */
static int
tlog_source_read_msg(struct tlog_source *source)
{
    int rc;
    struct json_object *obj;

    assert(tlog_source_is_valid(source));
    assert(tlog_msg_is_void(&source->msg));

    for (; ; tlog_msg_cleanup(&source->msg)) {
        rc = tlog_reader_read(source->reader, &obj);
        if (rc != 0)
            return rc;
        if (obj == NULL)
            return 0;

        if (!tlog_msg_init(&source->msg, obj))
            return TLOG_SOURCE_ERROR_INVALID_OBJECT;

        if (source->hostname != NULL &&
            strcmp(source->msg.host, source->hostname) != 0)
            continue;
        if (source->username != NULL &&
            strcmp(source->msg.user, source->username) != 0)
            continue;
        if (source->session_id != 0 &&
            source->msg.session != source->session_id)
            continue;

        return 0;
    }
}

int
tlog_source_read(struct tlog_source *source, struct tlog_pkt *pkt)
{
    int rc;

    assert(tlog_source_is_valid(source));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));

    while (true) {
        if (tlog_msg_is_void(&source->msg)) {
            rc = tlog_source_read_msg(source);
            if (rc != 0)
                return rc;
            if (tlog_msg_is_void(&source->msg))
                return 0;
        }

        if (!tlog_msg_read(&source->msg, pkt,
                           source->io_buf, source->io_size)) {
            tlog_msg_cleanup(&source->msg);
            return TLOG_SOURCE_ERROR_INVALID_OBJECT;
        }

        if (tlog_pkt_is_void(pkt)) {
            tlog_msg_cleanup(&source->msg);
        } else {
            return 0;
        }
    }
}
