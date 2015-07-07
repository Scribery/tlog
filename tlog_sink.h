/*
 * Tlog sink.
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

#ifndef _TLOG_SINK_H
#define _TLOG_SINK_H

#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include "tlog_rc.h"
#include "tlog_io.h"
#include "tlog_writer.h"

/** Minimum value of I/O buffer size */
#define TLOG_SINK_IO_SIZE_MIN   TLOG_IO_SIZE_MIN

/* Sink instance */
struct tlog_sink {
    struct tlog_writer *writer;         /**< Log message writer */
    char               *hostname;       /**< Hostname */
    char               *username;       /**< Username */
    unsigned int        session_id;     /**< Session ID */
    size_t              message_id;     /**< Next message ID */
    struct timespec     start;          /**< Sink creation timestamp */
    struct tlog_io      io;             /**< I/O buffer and state */
    uint8_t            *message_buf;    /**< Message buffer pointer */
    size_t              message_len;    /**< Message buffer length */
};

/**
 * Check if a sink is valid.
 *
 * @param sink      The sink to check.
 *
 * @return True if the sink is valid, false otherwise.
 */
extern bool tlog_sink_is_valid(const struct tlog_sink *sink);

/**
 * Initialize a log sink.
 *
 * @param sink              Pointer to the sink to initialize.
 * @param writer            Log message writer.
 * @param hostname          Hostname to use in log messages.
 * @param session_id        Session ID to use in log messages.
 * @param io_size           Maximum I/O message payload length.
 * @param timestamp         Sink start timestamp.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_init(struct tlog_sink *sink,
                              struct tlog_writer *writer,
                              const char *hostname,
                              const char *username,
                              unsigned int session_id,
                              size_t io_size,
                              const struct timespec *timestamp);

/**
 * Create (allocate and initialize) a log sink.
 *
 * @param psink             Location for created sink pointer.
 * @param writer            Log message writer.
 * @param hostname          Hostname to use in log messages.
 * @param session_id        Session ID to use in log messages.
 * @param io_size           Maximum I/O message payload length.
 * @param timestamp         Sink start timestamp.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_create(struct tlog_sink **psink,
                                struct tlog_writer *writer,
                                const char *hostname,
                                const char *username,
                                unsigned int session_id,
                                size_t io_size,
                                const struct timespec *timestamp);

/**
 * Write window size to a log sink.
 *
 * @param sink      Pointer to the sink to write window size to.
 * @param timestamp Timestamp of the window change.
 * @param width     Window width in characters.
 * @param height    Window height in characters.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_window_write(struct tlog_sink *sink,
                                      const struct timespec *timestamp,
                                      unsigned short int width,
                                      unsigned short int height);

/**
 * Write terminal I/O to a log sink.
 *
 * @param sink      Pointer to the sink to write I/O to.
 * @param timestamp Timestamp of the I/O arrival.
 * @param output    True if writing output, false if input.
 * @param ptr       Input buffer pointer.
 * @param len       Input buffer length.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_io_write(struct tlog_sink *sink,
                                  const struct timespec *timestamp,
                                  bool output, const uint8_t *buf, size_t len);

/**
 * Cut a sink I/O - write pending incomplete characters.
 *
 * @param sink  The sink to cut I/O for.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_io_cut(struct tlog_sink *sink);

/**
 * Flush I/O pending in a log sink, on a character boundary.
 *
 * @param sink  The sink to flush.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_io_flush(struct tlog_sink *sink);

/**
 * Cleanup a log sink. Can be called more than once.
 *
 * @param sink  Pointer to the sink to cleanup.
 */
extern void tlog_sink_cleanup(struct tlog_sink *sink);

/**
 * Destroy (cleanup and free) a log sink.
 *
 * @param sink  Pointer to the sink to destroy, can be NULL.
 */
extern void tlog_sink_destroy(struct tlog_sink *sink);

#endif /* _TLOG_SINK_H */
