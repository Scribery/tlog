/*
 * Abstract sink.
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

#include <tlog/sink.h>
#include <tlog/rc.h>
#include <assert.h>

tlog_grc
tlog_sink_create(struct tlog_sink **psink,
                 const struct tlog_sink_type *type,
                 ...)
{
    va_list ap;
    tlog_grc grc;
    struct tlog_sink *sink;

    assert(psink != NULL);
    assert(tlog_sink_type_is_valid(type));
    assert(type->size >= sizeof(*sink));

    sink = calloc(type->size, 1);
    if (sink == NULL) {
        grc = TLOG_GRC_ERRNO;
    } else {
        sink->type = type;

        va_start(ap, type);
        grc = type->init(sink, ap);
        va_end(ap);

        if (grc == TLOG_RC_OK) {
            assert(tlog_sink_is_valid(sink));
        } else {
            free(sink);
            sink = NULL;
        }
    }

    *psink = sink;
    return grc;
}

bool
tlog_sink_is_valid(const struct tlog_sink *sink)
{
    return sink != NULL &&
           tlog_sink_type_is_valid(sink->type) && (
               sink->type->is_valid == NULL ||
               sink->type->is_valid(sink)
           );
}

tlog_grc
tlog_sink_write(struct tlog_sink *sink,
                const struct tlog_pkt *pkt,
                struct tlog_pkt_pos *ppos,
                const struct tlog_pkt_pos *end)
{
    tlog_grc grc;
    assert(tlog_sink_is_valid(sink));
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));
    struct tlog_pkt_pos pkt_end = TLOG_PKT_POS_VOID;

    if (end == NULL) {
        tlog_pkt_pos_move_past(&pkt_end, pkt);
        end = &pkt_end;
    } else {
        assert(tlog_pkt_pos_is_valid(end));
        assert(tlog_pkt_pos_is_compatible(end, pkt));
        assert(tlog_pkt_pos_is_reachable(end, pkt));
    }

    if (ppos == NULL) {
        struct tlog_pkt_pos pos = TLOG_PKT_POS_VOID;
        do {
            grc = sink->type->write(sink, pkt, &pos, end);
        } while ((grc == TLOG_RC_OK ||
                  grc == TLOG_GRC_FROM(errno, EINTR)) &&
                 tlog_pkt_pos_is_in(&pos, pkt));
    } else {
        assert(tlog_pkt_pos_is_valid(ppos));
        assert(tlog_pkt_pos_is_compatible(ppos, pkt));
        assert(tlog_pkt_pos_is_reachable(ppos, pkt));
        assert(tlog_pkt_pos_cmp(ppos, end) <= 0);
        grc = sink->type->write(sink, pkt, ppos, end);
    }

    assert(tlog_sink_is_valid(sink));
    return grc;
}

tlog_grc
tlog_sink_cut(struct tlog_sink *sink)
{
    tlog_grc grc;
    assert(tlog_sink_is_valid(sink));

    grc = (sink->type->cut != NULL) ? sink->type->cut(sink)
                                    : TLOG_RC_OK;

    assert(tlog_sink_is_valid(sink));
    return grc;
}

tlog_grc
tlog_sink_flush(struct tlog_sink *sink)
{
    tlog_grc grc;
    assert(tlog_sink_is_valid(sink));

    grc = (sink->type->flush != NULL) ? sink->type->flush(sink)
                                       : TLOG_RC_OK;

    assert(tlog_sink_is_valid(sink));
    return grc;
}

void
tlog_sink_destroy(struct tlog_sink *sink)
{
    assert(sink == NULL || tlog_sink_is_valid(sink));

    if (sink == NULL) {
        return;
    }
    if (sink->type->cleanup != NULL) {
        sink->type->cleanup(sink);
    }
    free(sink);
}
