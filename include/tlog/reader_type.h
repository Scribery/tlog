/*
 * Tlog message reader type.
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

#ifndef _TLOG_READER_TYPE_H
#define _TLOG_READER_TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <json.h>
#include "tlog/grc.h"

/* Forward declaration */
struct tlog_reader;

/**
 * Init function prototype.
 *
 * @param reader    The reader to initialize, must be cleared to zero.
 * @param ap        Argument list.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_reader_type_init_fn)(struct tlog_reader *reader,
                                             va_list ap);

/**
 * Validation function prototype.
 *
 * @param reader    The reader to operate on.
 *
 * @return True if the reader is valid, false otherwise
 */
typedef bool (*tlog_reader_type_is_valid_fn)(const struct tlog_reader
                                                               *reader);

/**
 * Location retrieval function prototype.
 *
 * @param reader    The reader to retrieve current location from.
 *
 * @return Opaque location value.
 */
typedef size_t (*tlog_reader_type_loc_get_fn)(const struct tlog_reader
                                                               *reader);

/**
 * Location formatting function prototype.
 *
 * @param loc   Opaque location value to format.
 *
 * @return Dynamically-allocated string describing the location, or NULL in
 *         case of error (see errno).
 */
typedef char * (*tlog_reader_type_loc_fmt_fn)(size_t loc);

/**
 * Message reading function prototype.
 *
 * @param reader    The reader to operate on.
 * @param pobject   Location for a JSON object, set to NULL on end of stream;
 *                  call json_object_put after the returned object is no
 *                  longer needed.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_reader_type_read_fn)(struct tlog_reader *reader,
                                             struct json_object **pobject);

/**
 * Cleanup function prototype.
 *
 * @param reader    The reader to cleanup.
 */
typedef void (*tlog_reader_type_cleanup_fn)(struct tlog_reader *reader);

/* Writer type */
struct tlog_reader_type {
    size_t                          size;       /**< Instance size */
    tlog_reader_type_init_fn        init;       /**< Init function */
    tlog_reader_type_is_valid_fn    is_valid;   /**< Validation function */
    tlog_reader_type_loc_get_fn     loc_get;    /**< Location retrieval
                                                     function */
    tlog_reader_type_loc_fmt_fn     loc_fmt;    /**< Location formatting
                                                     function */
    tlog_reader_type_read_fn        read;       /**< Reading function */
    tlog_reader_type_cleanup_fn     cleanup;    /**< Cleanup function */
};

/**
 * Check if a reader type is valid.
 *
 * @param type  The reader type to check.
 *
 * @return True if the reader type is valid.
 */
extern bool tlog_reader_type_is_valid(const struct tlog_reader_type *type);

/**
 * Format a reader type location.
 *
 * @param reader_type   The reader type to format location for.
 * @param loc           Opaque location value to format.
 *
 * @return Dynamically-allocated string describing the location, or NULL in
 *         case of error (see errno).
 */
extern char *tlog_reader_type_loc_fmt(const struct tlog_reader_type *type,
                                      size_t loc);

#endif /* _TLOG_READER_TYPE_H */
