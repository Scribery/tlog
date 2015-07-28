/*
 * Tlog abstract message reader.
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
#include <errno.h>
#include "tlog/reader.h"

int
tlog_reader_create(struct tlog_reader **preader,
                   const struct tlog_reader_type *type,
                   ...)
{
    va_list ap;
    struct tlog_reader *reader;
    int rc;

    assert(preader != NULL);
    assert(tlog_reader_type_is_valid(type));
    assert(type->size >= sizeof(*reader));

    reader = malloc(type->size);
    if (reader == NULL) {
        rc = -errno;
    } else {
        reader->type = type;

        va_start(ap, type);
        rc = type->init(reader, ap);
        va_end(ap);

        if (rc == 0) {
            assert(tlog_reader_is_valid(reader));
        } else {
            free(reader);
            reader = NULL;
        }
    }

    *preader = reader;
    return rc;
}

bool
tlog_reader_is_valid(const struct tlog_reader *reader)
{
    return reader != NULL &&
           tlog_reader_type_is_valid(reader->type) && (
               reader->type->is_valid == NULL ||
               reader->type->is_valid(reader)
           );
}

char *
tlog_reader_strerror(const struct tlog_reader *reader, int error)
{
    assert(tlog_reader_is_valid(reader));
    return tlog_reader_type_strerror(reader->type, error);
}

size_t
tlog_reader_loc_get(const struct tlog_reader *reader)
{
    assert(tlog_reader_is_valid(reader));
    return (reader->type->loc_get != NULL)
                ? reader->type->loc_get(reader)
                : 0;
}

char *
tlog_reader_loc_fmt(const struct tlog_reader *reader, size_t loc)
{
    assert(tlog_reader_is_valid(reader));
    return tlog_reader_type_loc_fmt(reader->type, loc);
}

int
tlog_reader_read(struct tlog_reader *reader,
                 struct json_object **pobject)
{
    int rc;
    assert(tlog_reader_is_valid(reader));
    assert(pobject != NULL);
    rc = reader->type->read(reader, pobject);
    if (rc != 0)
        *pobject = NULL;
    assert(tlog_reader_is_valid(reader));
    return rc;
}

void
tlog_reader_destroy(struct tlog_reader *reader)
{
    assert(reader == NULL || tlog_reader_is_valid(reader));

    if (reader == NULL)
        return;
    if (reader->type->cleanup != NULL)
        reader->type->cleanup(reader);
    free(reader);
}
