/**
 * @file
 * @brief Terminal data source type.
 *
 * A structure describing a terminal data source implementation.
 */
/*
 * Copyright (C) 2016 Red Hat
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

#ifndef _TLOG_SOURCE_TYPE_H
#define _TLOG_SOURCE_TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <tlog/grc.h>
#include <tlog/pkt.h>

/* Forward declaration */
struct tlog_source;

/**
 * Init function prototype.
 *
 * @param source    The source to initialize, must be cleared to zero.
 * @param ap        Argument list.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_source_type_init_fn)(struct tlog_source *source,
                                             va_list ap);

/**
 * Validation function prototype.
 *
 * @param source    The source to operate on.
 *
 * @return True if the source is valid, false otherwise
 */
typedef bool (*tlog_source_type_is_valid_fn)(
                            const struct tlog_source *source);

/**
 * Location retrieval function prototype.
 *
 * @param source    The source to retrieve current location from.
 *
 * @return Opaque location value.
 */
typedef size_t (*tlog_source_type_loc_get_fn)(
                            const struct tlog_source *source);

/**
 * Location formatting function prototype.
 *
 * @param source    The source to format location for.
 * @param loc       Opaque location value to format.
 *
 * @return Dynamically-allocated string describing the location, or NULL in
 *         case of error (see errno).
 */
typedef char * (*tlog_source_type_loc_fmt_fn)(
                            const struct tlog_source *source,
                            size_t loc);

/**
 * Packet reading function prototype.
 *
 * @param source    The source to operate on.
 * @param pkt       The packet to write the received data into, must be void,
 *                  will be void on end-of-stream, or in case of error.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_source_type_read_fn)(struct tlog_source *source,
                                             struct tlog_pkt *pkt);

/**
 * Cleanup function prototype.
 *
 * @param source    The source to cleanup.
 */
typedef void (*tlog_source_type_cleanup_fn)(struct tlog_source *source);

/** Source type */
struct tlog_source_type {
    size_t                          size;       /**< Instance size */
    tlog_source_type_init_fn        init;       /**< Init function */
    tlog_source_type_is_valid_fn    is_valid;   /**< Validation function */
    tlog_source_type_loc_get_fn     loc_get;    /**< Location retrieval
                                                     function */
    tlog_source_type_loc_fmt_fn     loc_fmt;    /**< Location formatting
                                                     function */
    tlog_source_type_read_fn        read;       /**< Reading function */
    tlog_source_type_cleanup_fn     cleanup;    /**< Cleanup function */
};

/**
 * Check if a source type is valid.
 *
 * @param type  The source type to check.
 *
 * @return True if the source type is valid.
 */
static inline bool
tlog_source_type_is_valid(const struct tlog_source_type *type)
{
    return type != NULL &&
           type->size != 0 &&
           type->init != NULL &&
           (type->loc_fmt == NULL || type->loc_get != NULL) &&
           type->read != NULL;
}

#endif /* _TLOG_SOURCE_TYPE_H */
