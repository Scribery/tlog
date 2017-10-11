/**
 * @file
 * @brief Abstract JSON log message writer.
 *
 * An abstract interface for a JSON log message writer.
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

#ifndef _TLOG_JSON_WRITER_H
#define _TLOG_JSON_WRITER_H

#include <tlog/json_writer_type.h>

/** Abstract writer */
struct tlog_json_writer {
    const struct tlog_json_writer_type *type;   /**< Type */
};

/**
 * Allocate and initialize a writer of the specified type, with specified
 * arguments. See the particular type description for specific arguments
 * required.
 *
 * @param pwriter   Location for the created writer pointer, will be set to
 *                  NULL in case of error.
 * @param type      The type of writer to create.
 * @param ...       The type-specific writer creation arguments.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_json_writer_create(
                            struct tlog_json_writer **pwriter,
                            const struct tlog_json_writer_type *type,
                            ...);

/**
 * Check if a writer is valid.
 *
 * @param writer    The writer to check.
 *
 * @return True if the writer is valid.
 */
extern bool tlog_json_writer_is_valid(const struct tlog_json_writer *writer);

/**
 * Write with a writer.
 * Atomic, i.e. always writes everything, or nothing,
 * unless an error beside EINTR occurs.
 *
 * @param writer    The writer to write with.
 * @param id        ID of the message in the buffer. Cannot be zero.
 * @param buf       The pointer to the message buffer to write.
 * @param len       The length of the message buffer to write.
 *
 * @return Global return code.
 *         Can return TLOG_GRC_FROM(errno, EINTR), if writing was interrupted
 *         by a signal before anything was written.
 */
extern tlog_grc tlog_json_writer_write(struct tlog_json_writer *writer,
                                       size_t id,
                                       const uint8_t *buf, size_t len);

/**
 * Cleanup and deallocate a writer.
 *
 * @param writer    The writer to destroy, can be NULL.
 */
extern void tlog_json_writer_destroy(struct tlog_json_writer *writer);

#endif /* _TLOG_JSON_WRITER_H */
