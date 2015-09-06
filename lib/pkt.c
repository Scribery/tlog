/*
 * Tlog packet.
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
#include <tlog/pkt.h>

const char *
tlog_pkt_type_to_str(enum tlog_pkt_type type)
{
    assert(tlog_pkt_type_is_valid(type));
    switch (type) {
    case TLOG_PKT_TYPE_VOID:
        return "void";
    case TLOG_PKT_TYPE_WINDOW:
        return "window";
    case TLOG_PKT_TYPE_IO:
        return "I/O";
    default:
        return "unknown";
    }
}

void
tlog_pkt_init(struct tlog_pkt *pkt)
{
    assert(pkt != NULL);
    memset(pkt, 0, sizeof(*pkt));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));
}

void
tlog_pkt_init_window(struct tlog_pkt *pkt,
                     const struct timespec *timestamp,
                     unsigned short int width,
                     unsigned short int height)
{
    assert(pkt != NULL);
    assert(timestamp != NULL);
    memset(pkt, 0, sizeof(*pkt));
    pkt->timestamp = *timestamp;
    pkt->type = TLOG_PKT_TYPE_WINDOW;
    pkt->data.window.width = width;
    pkt->data.window.height = height;
    assert(tlog_pkt_is_valid(pkt));
}

void
tlog_pkt_init_io(struct tlog_pkt *pkt,
                 const struct timespec *timestamp,
                 bool output,
                 uint8_t *buf,
                 bool buf_owned,
                 size_t len)
{
    assert(pkt != NULL);
    assert(timestamp != NULL);
    assert(buf != NULL || len == 0);
    memset(pkt, 0, sizeof(*pkt));
    pkt->timestamp = *timestamp;
    pkt->type = TLOG_PKT_TYPE_IO;
    pkt->data.io.output = output;
    pkt->data.io.buf = buf;
    pkt->data.io.buf_owned = buf_owned;
    pkt->data.io.len = len;
    assert(tlog_pkt_is_valid(pkt));
}

bool
tlog_pkt_is_valid(const struct tlog_pkt *pkt)
{
    if (!(pkt != NULL &&
          tlog_pkt_type_is_valid(pkt->type)))
        return false;

    switch (pkt->type) {
    case TLOG_PKT_TYPE_IO:
        if (!(pkt->data.io.buf != NULL || pkt->data.io.len == 0))
            return false;
        break;
    default:
        break;
    }

    return true;
}

bool
tlog_pkt_is_void(const struct tlog_pkt *pkt)
{
    assert(tlog_pkt_is_valid(pkt));
    return pkt->type == TLOG_PKT_TYPE_VOID;
}

bool
tlog_pkt_is_equal(const struct tlog_pkt *a, const struct tlog_pkt *b)
{
    assert(tlog_pkt_is_valid(a));
    assert(tlog_pkt_is_valid(b));

    if (a->type != b->type ||
        a->timestamp.tv_sec != b->timestamp.tv_sec ||
        a->timestamp.tv_nsec != b->timestamp.tv_nsec)
        return false;

    switch (a->type) {
    case TLOG_PKT_TYPE_WINDOW:
        if (a->data.window.width != b->data.window.width ||
            a->data.window.height != b->data.window.height)
            return false;
        break;
    case TLOG_PKT_TYPE_IO:
        if (a->data.io.output != b->data.io.output ||
            a->data.io.len != b->data.io.len ||
            memcmp(a->data.io.buf, b->data.io.buf, a->data.io.len) != 0)
            return false;
        break;
    default:
        break;
    }

    return true;
}

void
tlog_pkt_cleanup(struct tlog_pkt *pkt)
{
    assert(tlog_pkt_is_valid(pkt));

    switch (pkt->type) {
    case TLOG_PKT_TYPE_IO:
        if (pkt->data.io.buf_owned)
            free(pkt->data.io.buf);
        break;
    default:
        break;
    }

    tlog_pkt_init(pkt);
    assert(tlog_pkt_is_valid(pkt));
}
