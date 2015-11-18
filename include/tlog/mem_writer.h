/*
 * Tlog memory buffer message writer.
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

#ifndef _TLOG_MEM_WRITER_H
#define _TLOG_MEM_WRITER_H

#include <assert.h>
#include <tlog/writer.h>

/**
 * File descriptor message writer type
 *
 * Creation arguments:
 *
 * char   **pbuf    Location for the allocated buffer pointer.
 * size_t  *plen    Location for the length of the buffer contents. The memory
 *                  allocated for the buffer can be larger.
 */
extern const struct tlog_writer_type tlog_mem_writer_type;

/**
 * Create an instance of memory buffer writer.
 *
 * @param pbuf  Location for the allocated buffer pointer.
 * @param plen  Location for the length of the buffer contents. The memory
 *              allocated for the buffer can be larger.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_mem_writer_create(struct tlog_writer **pwriter, char **pbuf, size_t *plen)
{
    assert(pbuf != NULL);
    assert(plen != NULL);
    return tlog_writer_create(pwriter, &tlog_mem_writer_type, pbuf, plen);
}

#endif /* _TLOG_MEM_WRITER_H */
