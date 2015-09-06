/*
 * Tlog message writer type.
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

#ifndef _TLOG_WRITER_TYPE_H
#define _TLOG_WRITER_TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <tlog/grc.h>

/* Forward declaration */
struct tlog_writer;

/**
 * Init function prototype.
 *
 * @param writer    The writer to initialize, must be cleared to zero.
 * @param ap        Argument list.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_writer_type_init_fn)(struct tlog_writer *writer,
                                             va_list ap);

/**
 * Validation function prototype.
 *
 * @param writer    The writer to operate on.
 *
 * @return True if the writer is valid, false otherwise
 */
typedef bool (*tlog_writer_type_is_valid_fn)(const struct tlog_writer
                                                               *writer);

/**
 * Message writing function prototype.
 *
 * @param writer    The writer to operate on.
 * @param buf       The pointer to the message buffer to write.
 * @param len       The length of the message buffer to write.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_writer_type_write_fn)(struct tlog_writer *writer,
                                              const uint8_t *buf,
                                              size_t len);

/**
 * Cleanup function prototype.
 *
 * @param writer    The writer to cleanup.
 */
typedef void (*tlog_writer_type_cleanup_fn)(struct tlog_writer *writer);

/* Writer type */
struct tlog_writer_type {
    size_t                          size;       /**< Instance size */
    tlog_writer_type_init_fn        init;       /**< Init function */
    tlog_writer_type_is_valid_fn    is_valid;   /**< Validation function */
    tlog_writer_type_write_fn       write;      /**< Writing function */
    tlog_writer_type_cleanup_fn     cleanup;    /**< Cleanup function */
};

/**
 * Check if a writer type is valid.
 *
 * @param writer    The writer to check.
 *
 * @return True if the writer is valid.
 */
static inline bool
tlog_writer_type_is_valid(const struct tlog_writer_type *type)
{
    return type != NULL &&
           type->size != 0 &&
           type->init != NULL &&
           type->write != NULL;
}

#endif /* _TLOG_WRITER_TYPE_H */
