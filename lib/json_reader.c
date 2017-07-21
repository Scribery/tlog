/*
 * Abstract message reader.
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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <tlog/json_reader.h>
#include <tlog/rc.h>

tlog_grc
tlog_json_reader_create(struct tlog_json_reader **preader,
                        const struct tlog_json_reader_type *type,
                        ...)
{
    va_list ap;
    struct tlog_json_reader *reader;
    tlog_grc grc;

    assert(preader != NULL);
    assert(tlog_json_reader_type_is_valid(type));
    assert(type->size >= sizeof(*reader));

    reader = calloc(type->size, 1);
    if (reader == NULL) {
        grc = TLOG_GRC_ERRNO;
    } else {
        reader->type = type;

        va_start(ap, type);
        grc = type->init(reader, ap);
        va_end(ap);

        if (grc == TLOG_RC_OK) {
            assert(tlog_json_reader_is_valid(reader));
        } else {
            free(reader);
            reader = NULL;
        }
    }

    *preader = reader;
    return grc;
}

bool
tlog_json_reader_is_valid(const struct tlog_json_reader *reader)
{
    return reader != NULL &&
           tlog_json_reader_type_is_valid(reader->type) && (
               reader->type->is_valid == NULL ||
               reader->type->is_valid(reader)
           );
}

size_t
tlog_json_reader_loc_get(const struct tlog_json_reader *reader)
{
    assert(tlog_json_reader_is_valid(reader));
    return (reader->type->loc_get != NULL)
                ? reader->type->loc_get(reader)
                : 0;
}

char *
tlog_json_reader_loc_fmt(const struct tlog_json_reader *reader, size_t loc)
{
    assert(tlog_json_reader_is_valid(reader));
    if (reader->type->loc_fmt != NULL) {
        return reader->type->loc_fmt(reader, loc);
    } else if (reader->type->loc_get != NULL) {
        char *str;
        return asprintf(&str, "location %zu", loc) >= 0 ? str : NULL;
    } else {
        return strdup("unknown location");
    }
}

tlog_grc
tlog_json_reader_read(struct tlog_json_reader *reader,
                      struct json_object **pobject)
{
    tlog_grc grc;
#ifndef NDEBUG
    struct json_object *orig_object;
#endif
    assert(tlog_json_reader_is_valid(reader));
    assert(pobject != NULL);
#ifndef NDEBUG
    orig_object = *pobject;
#endif
    grc = reader->type->read(reader, pobject);
    assert(grc == TLOG_RC_OK || *pobject == orig_object);
    assert(tlog_json_reader_is_valid(reader));
    return grc;
}

void
tlog_json_reader_destroy(struct tlog_json_reader *reader)
{
    assert(reader == NULL || tlog_json_reader_is_valid(reader));

    if (reader == NULL) {
        return;
    }
    if (reader->type->cleanup != NULL) {
        reader->type->cleanup(reader);
    }
    free(reader);
}
