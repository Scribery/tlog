/**
 * @file
 * @brief Abstract terminal data source.
 *
 * Abstract terminal data source allows creation and usage of sources of
 * specific types.
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

#ifndef _TLOG_SOURCE_H
#define _TLOG_SOURCE_H

#include <tlog/source_type.h>

/** Abstract source */
struct tlog_source {
    const struct tlog_source_type *type;   /**< Type */
};

/**
 * Allocate and initialize a source of the specified type, with specified
 * arguments. See the particular type description for specific arguments
 * required.
 *
 * @param psource   Location for the created source pointer, will be set to
 *                  NULL in case of error.
 * @param type      The type of source to create.
 * @param ...       The type-specific source creation arguments.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_source_create(struct tlog_source **psource,
                                   const struct tlog_source_type *type,
                                   ...);

/**
 * Check if a source is valid.
 *
 * @param source    The source to check.
 *
 * @return True if the source is valid.
 */
extern bool tlog_source_is_valid(const struct tlog_source *source);

/**
 * Retrieve current opaque location of the source.
 *
 * @param source    The source to retrieve current location from.
 *
 * @return Opaque location value.
 */
extern size_t tlog_source_loc_get(const struct tlog_source *source);

/**
 * Format opaque location as a string.
 *
 * @param source    The source to format location for.
 * @param loc       Opaque location value to format.
 *
 * @return Dynamically-allocated string describing the location, or NULL in
 *         case of error (see errno).
 */
extern char *tlog_source_loc_fmt(const struct tlog_source *source,
                                 size_t loc);

/**
 * Read a packet from the source.
 *
 * @param source    The source to read from.
 * @param pkt       The packet to write the received data into, must be void,
 *                  will be void on end-of-stream.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_source_read(struct tlog_source *source,
                                 struct tlog_pkt *pkt);

/**
 * Destroy (cleanup and free) a log source.
 *
 * @param source  Pointer to the source to destroy, can be NULL.
 */
extern void tlog_source_destroy(struct tlog_source *source);

#endif /* _TLOG_SOURCE_H */
