/*
 * Tlog memory buffer message reader.
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

#ifndef _TLOG_MEM_READER_H
#define _TLOG_MEM_READER_H

#include <assert.h>
#include <tlog/reader.h>

/**
 * Memory buffer message reader type
 *
 * Creation arguments:
 *
 * const char  *buf Pointer to the buffer to read from.
 * size_t       len Length of the buffer to read from.
 */
extern const struct tlog_reader_type tlog_mem_reader_type;

/**
 * Create a memory buffer reader.
 *
 * @param preader   Location for the created reader pointer, will be set to
 *                  NULL in case of error.
 * @param buf       Pointer to the buffer to read from.
 * @param len       Length of the buffer to read from.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_mem_reader_create(struct tlog_reader **preader,
                       const char *buf, size_t len)
{
    assert(preader != NULL);
    assert(buf != NULL || len == 0);
    return tlog_reader_create(preader, &tlog_mem_reader_type, buf, len);
}

#endif /* _TLOG_MEM_READER_H */
