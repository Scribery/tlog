/*
 * Memory buffer JSON message writer.
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <tlog/rc.h>
#include <tlog/mem_json_writer.h>

/** Memory buffer writer data */
struct tlog_mem_json_writer {
    struct tlog_json_writer writer; /**< Abstract writer instance */
    char                  **pbuf;   /**< Location for the buffer pointer */
    size_t                 *plen;   /**< Location for the buffer length */
    size_t                  size;   /**< Allocated size of the buffer */
};

static tlog_grc
tlog_mem_json_writer_init(struct tlog_json_writer *writer, va_list ap)
{
    struct tlog_mem_json_writer *mem_json_writer =
                                    (struct tlog_mem_json_writer*)writer;
    mem_json_writer->pbuf = va_arg(ap, char **);
    mem_json_writer->plen = va_arg(ap, size_t *);
    mem_json_writer->size = 256;

    *mem_json_writer->plen = 0;
    *mem_json_writer->pbuf = malloc(mem_json_writer->size);
    if (*mem_json_writer->pbuf == NULL) {
        return TLOG_GRC_ERRNO;
    }
    return TLOG_RC_OK;
}

static tlog_grc
tlog_mem_json_writer_write(struct tlog_json_writer *writer,
                           size_t id, const uint8_t *buf, size_t len)
{
    struct tlog_mem_json_writer *mem_json_writer =
                                    (struct tlog_mem_json_writer*)writer;
    size_t new_len = *mem_json_writer->plen + len;

    (void)id;

    if (new_len > mem_json_writer->size) {
        size_t new_size;
        char *new_buf;
        for (new_size = mem_json_writer->size;
             new_size < new_len;
             new_size *= 2);
        new_buf = realloc(*mem_json_writer->pbuf, new_size);
        if (new_buf == NULL) {
            return TLOG_GRC_ERRNO;
        }
        *mem_json_writer->pbuf = new_buf;
        mem_json_writer->size = new_size;
    }

    memcpy(*mem_json_writer->pbuf + *mem_json_writer->plen, buf, len);
    *mem_json_writer->plen = new_len;
    return TLOG_RC_OK;
}

const struct tlog_json_writer_type tlog_mem_json_writer_type = {
    .size   = sizeof(struct tlog_mem_json_writer),
    .init   = tlog_mem_json_writer_init,
    .write  = tlog_mem_json_writer_write,
};
