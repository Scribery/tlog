/*
 * Abstract source.
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

#include <config.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <tlog/source.h>
#ifndef NDEBUG
#include <tlog/timespec.h>
#include <tlog/delay.h>
#endif

tlog_grc
tlog_source_create(struct tlog_source **psource,
                   const struct tlog_source_type *type,
                   ...)
{
    va_list ap;
    struct tlog_source *source;
    tlog_grc grc;

    assert(psource != NULL);
    assert(tlog_source_type_is_valid(type));
    assert(type->size >= sizeof(*source));

    source = calloc(type->size, 1);
    if (source == NULL) {
        grc = TLOG_GRC_ERRNO;
    } else {
        source->type = type;

        va_start(ap, type);
        grc = type->init(source, ap);
        va_end(ap);

        if (grc == TLOG_RC_OK) {
            assert(tlog_source_is_valid(source));
        } else {
            free(source);
            source = NULL;
        }
    }

    *psource = source;
    return grc;
}

bool
tlog_source_is_valid(const struct tlog_source *source)
{
    return source != NULL &&
           tlog_source_type_is_valid(source->type) && (
               source->type->is_valid == NULL ||
               source->type->is_valid(source)
           );
}

size_t
tlog_source_loc_get(const struct tlog_source *source)
{
    assert(tlog_source_is_valid(source));
    return (source->type->loc_get != NULL)
                ? source->type->loc_get(source)
                : 0;
}

char *
tlog_source_loc_fmt(const struct tlog_source *source, size_t loc)
{
    assert(tlog_source_is_valid(source));
    if (source->type->loc_fmt != NULL) {
        return source->type->loc_fmt(source, loc);
    } else if (source->type->loc_get != NULL) {
        char *str;
        return asprintf(&str, "location %zu", loc) >= 0 ? str : NULL;
    } else {
        return strdup("unknown location");
    }
}

tlog_grc
tlog_source_read(struct tlog_source *source, struct tlog_pkt *pkt)
{
    tlog_grc grc;
#ifndef NDEBUG
    struct timespec diff;
#endif
    assert(tlog_source_is_valid(source));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));

    grc = source->type->read(source, pkt);

    assert(grc == TLOG_RC_OK || tlog_pkt_is_void(pkt));
    assert(tlog_source_is_valid(source));
#ifndef NDEBUG
    if (!tlog_pkt_is_void(pkt)) {
        tlog_timespec_sub(&pkt->timestamp, &source->last_timestamp, &diff);
        assert(tlog_timespec_cmp(&diff, &tlog_timespec_zero) >= 0);
        assert(tlog_timespec_cmp(&diff, &tlog_delay_max_timespec) <= 0);
        source->last_timestamp = pkt->timestamp;
    }
#endif
    return grc;
}

void
tlog_source_destroy(struct tlog_source *source)
{
    assert(source == NULL || tlog_source_is_valid(source));

    if (source == NULL) {
        return;
    }
    if (source->type->cleanup != NULL) {
        source->type->cleanup(source);
    }
    free(source);
}
