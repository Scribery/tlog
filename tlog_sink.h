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

/** Minimum value of io_max_len creation parameter */
#define TLOG_SINK_IO_MAX_LEN_MIN    64

/** I/O state and parameters of a sink instance */
struct tlog_sink_io {
    size_t              max_len;    /**< Maximum total data length and
                                         size of each buffer below */
    size_t              len;        /**< Total data length */

    uint8_t            *input_buf;  /**< Input buffer */
    size_t              input_len;  /**< Input length */

    uint8_t            *output_buf; /**< Output buffer */
    size_t              output_len; /**< Output length */

    uint8_t            *timing_buf; /**< Timing buffer */
    size_t              timing_len; /**< Timing length */

    struct timespec     first;      /**< First write timestamp */
    struct timespec     last;       /**< Last write timestamp */
};

/* Sink instance */
struct tlog_sink {
    int                     fd;             /**< Log output file descriptor */
    char                   *hostname;       /**< Hostname */
    unsigned int            session_id;     /**< Session ID */
    size_t                  message_id;     /**< Next message ID */
    clockid_t               clock_id;       /**< ID of the clock to use */
    struct timespec         start;          /**< Sink creation timestamp */
    struct tlog_sink_io     io;             /**< I/O state and parameters */
    uint8_t                *message_buf;    /**< Message buffer pointer */
    size_t                  message_len;    /**< Message buffer length */
};

/**
 * Check if a sink is valid.
 *
 * @param sink      The sink to check.
 *
 * @return True if the sink is valid, false otherwise.
 */
extern bool tlog_sink_valid(const struct tlog_sink *sink);

/**
 * Initialize a log sink.
 *
 * @param sink              Pointer to the sink to initialize.
 * @param filename          Name of the file to write the log to.
 * @param hostname          Hostname to use in log messages.
 * @param session_id        Session ID to use in log messages.
 * @param io_max_len        Maximum I/O message payload length.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_init(struct tlog_sink *sink,
                              const char *filename,
                              const char *hostname,
                              unsigned int session_id,
                              size_t io_max_len);

/**
 * Create (allocate and initialize) a log sink.
 *
 * @param psink             Location for created sink pointer.
 * @param filename          Name of the file to write the log to.
 * @param io_max_len        Maximum I/O message payload length.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_create(struct tlog_sink **psink,
                                const char *filename,
                                const char *hostname,
                                unsigned int session_id,
                                size_t io_max_len);

/**
 * Write window size to a log sink.
 *
 * @param sink      Pointer to the sink to write window size to.
 * @param width     Window width in characters.
 * @param height    Window height in characters.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_write_window(struct tlog_sink *sink,
                                      unsigned short int width,
                                      unsigned short int height);

/**
 * Write terminal input to a log sink.
 *
 * @param sink  Pointer to the sink to write input to.
 * @param ptr   Input buffer pointer.
 * @param len   Input buffer length.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_write_input(struct tlog_sink *sink,
                                     const uint8_t *buf, size_t len);

/**
 * Write terminal output to a log sink.
 *
 * @param sink  Pointer to the sink to write output to.
 * @param ptr   Output buffer pointer.
 * @param len   Output buffer length.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_write_output(struct tlog_sink *sink,
                                      const uint8_t *buf, size_t len);

/**
 * Flush any I/O data pending in a log sink.
 *
 * @param sink  The sink to flush.
 *
 * @return Status code.
 */
extern tlog_rc tlog_sink_flush(struct tlog_sink *sink);

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
