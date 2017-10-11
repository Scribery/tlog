/**
 * @file
 * @brief JSON log message writer type.
 *
 * A structure describing a JSON log message writer implementation.
 */
/*
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

#ifndef _TLOG_JSON_WRITER_TYPE_H
#define _TLOG_JSON_WRITER_TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <tlog/grc.h>

/* Forward declaration */
struct tlog_json_writer;

/**
 * Init function prototype.
 *
 * @param writer    The writer to initialize, must be cleared to zero.
 * @param ap        Argument list.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_json_writer_type_init_fn)(
                                struct tlog_json_writer *writer,
                                va_list ap);

/**
 * Validation function prototype.
 *
 * @param writer    The writer to operate on.
 *
 * @return True if the writer is valid, false otherwise
 */
typedef bool (*tlog_json_writer_type_is_valid_fn)(
                                const struct tlog_json_writer
                                *writer);

/**
 * Message writing function prototype.
 * Atomic, i.e. always writes everything, or nothing,
 * unless an error beside EINTR occurs.
 *
 * @param writer    The writer to operate on.
 * @param id        ID of the message in the buffer. Cannot be zero.
 * @param buf       The pointer to the message buffer to write.
 * @param len       The length of the message buffer to write.
 *
 * @return Global return code.
 *         Can return TLOG_GRC_FROM(errno, EINTR), if writing was interrupted
 *         by a signal before anything was written.
 */
typedef tlog_grc (*tlog_json_writer_type_write_fn)(
                                struct tlog_json_writer *writer,
                                size_t id,
                                const uint8_t *buf,
                                size_t len);

/**
 * Cleanup function prototype.
 *
 * @param writer    The writer to cleanup.
 */
typedef void (*tlog_json_writer_type_cleanup_fn)(
                                struct tlog_json_writer *writer);

/** Writer type */
struct tlog_json_writer_type {
    size_t                             size;       /**< Instance size */
    tlog_json_writer_type_init_fn      init;       /**< Init function */
    tlog_json_writer_type_is_valid_fn  is_valid;   /**< Validation function */
    tlog_json_writer_type_write_fn     write;      /**< Writing function */
    tlog_json_writer_type_cleanup_fn   cleanup;    /**< Cleanup function */
};

/**
 * Check if a writer type is valid.
 *
 * @param type  The writer type to check.
 *
 * @return True if the writer type is valid.
 */
static inline bool
tlog_json_writer_type_is_valid(const struct tlog_json_writer_type *type)
{
    return type != NULL &&
           type->size != 0 &&
           type->init != NULL &&
           type->write != NULL;
}

#endif /* _TLOG_JSON_WRITER_TYPE_H */
