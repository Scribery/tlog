/**
 * @file
 * @brief Abstract JSON message reader.
 *
 * An abstract interface for a JSON log message reader.
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

#ifndef _TLOG_JSON_READER_H
#define _TLOG_JSON_READER_H

#include <tlog/json_reader_type.h>

/** Abstract reader */
struct tlog_json_reader {
    const struct tlog_json_reader_type *type;   /**< Type */
};

/**
 * Allocate and initialize a reader of the specified type, with specified
 * arguments. See the particular type description for specific arguments
 * required.
 *
 * @param preader   Location for the created reader pointer, will be set to
 *                  NULL in case of error.
 * @param type      The type of reader to create.
 * @param ...       The type-specific reader creation arguments.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_json_reader_create(
                        struct tlog_json_reader **preader,
                        const struct tlog_json_reader_type *type,
                        ...);

/**
 * Check if a reader is valid.
 *
 * @param reader    The reader to check.
 *
 * @return True if the reader is valid.
 */
extern bool tlog_json_reader_is_valid(
                        const struct tlog_json_reader *reader);

/**
 * Retrieve current opaque location of the reader.
 *
 * @param reader    The reader to retrieve current location from.
 *
 * @return Opaque location value.
 */
extern size_t tlog_json_reader_loc_get(
                        const struct tlog_json_reader *reader);

/**
 * Format opaque location as a string.
 *
 * @param reader    The reader to format location for.
 * @param loc       Opaque location value to format.
 *
 * @return Dynamically-allocated string describing the location, or NULL in
 *         case of error (see errno).
 */
extern char *tlog_json_reader_loc_fmt(const struct tlog_json_reader *reader,
                                      size_t loc);

/**
 * Read with a reader.
 *
 * @param reader    The reader to operate on.
 * @param pobject   Location for a JSON object, set to NULL on error or end of
 *                  stream; call json_object_put after the returned object is
 *                  no longer needed.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_json_reader_read(struct tlog_json_reader *reader,
                                      struct json_object **pobject);

/**
 * Cleanup and deallocate a reader.
 *
 * @param reader    The reader to destroy, can be NULL.
 */
extern void tlog_json_reader_destroy(struct tlog_json_reader *reader);

#endif /* _TLOG_JSON_READER_H */
