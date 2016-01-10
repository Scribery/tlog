/*
 * Abstract message writer.
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
#include <tlog/rc.h>
#include <tlog/writer.h>

tlog_grc
tlog_writer_create(struct tlog_writer **pwriter,
                   const struct tlog_writer_type *type,
                   ...)
{
    va_list ap;
    tlog_grc grc;
    struct tlog_writer *writer;

    assert(pwriter != NULL);
    assert(tlog_writer_type_is_valid(type));
    assert(type->size >= sizeof(*writer));

    writer = calloc(type->size, 1);
    if (writer == NULL) {
        grc = TLOG_GRC_ERRNO;
    } else {
        writer->type = type;

        va_start(ap, type);
        grc = type->init(writer, ap);
        va_end(ap);

        if (grc == TLOG_RC_OK) {
            assert(tlog_writer_is_valid(writer));
        } else {
            free(writer);
            writer = NULL;
        }
    }

    *pwriter = writer;
    return grc;
}

bool
tlog_writer_is_valid(const struct tlog_writer *writer)
{
    return writer != NULL &&
           tlog_writer_type_is_valid(writer->type) && (
               writer->type->is_valid == NULL ||
               writer->type->is_valid(writer)
           );
}

tlog_grc
tlog_writer_write(struct tlog_writer *writer,
                  const uint8_t *buf, size_t len)
{
    assert(tlog_writer_is_valid(writer));
    assert(buf != NULL || len == 0);

    return writer->type->write(writer, buf, len);
}

void
tlog_writer_destroy(struct tlog_writer *writer)
{
    assert(writer == NULL || tlog_writer_is_valid(writer));

    if (writer == NULL)
        return;
    if (writer->type->cleanup != NULL)
        writer->type->cleanup(writer);
    free(writer);
}
