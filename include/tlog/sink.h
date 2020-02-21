/**
 * @file
 * @brief Abstract terminal data sink.
 *
 * Abstract terminal data sink interface allows creation and usage of sinks of
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

#ifndef _TLOG_SINK_H
#define _TLOG_SINK_H

#include <tlog/sink_type.h>

/** Abstract sink */
struct tlog_sink {
    const struct tlog_sink_type    *type;   /**< Type */
};

/**
 * Allocate and initialize a sink of the specified type, with specified
 * arguments. See the particular type description for specific arguments
 * required.
 *
 * @param psink Location for the created sink pointer, will be set to NULL in
 *              case of error.
 * @param type  The type of sink to create.
 * @param ...   The type-specific sink creation arguments.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_sink_create(struct tlog_sink **psink,
                                 const struct tlog_sink_type *type,
                                 ...);

/**
 * Check if a sink is valid.
 *
 * @param sink      The sink to check.
 *
 * @return True if the sink is valid, false otherwise.
 */
extern bool tlog_sink_is_valid(const struct tlog_sink *sink);

/**
 * Write a packet to a sink.
 *
 * @param sink  Pointer to the sink to write the packet to.
 * @param pkt   The packet to write.
 * @param ppos  Location of position in the packet the write should start at
 *              (set to TLOG_PKT_POS_VOID at the start) / location for
 *              position offset in the packet the write ended at.
 *              If NULL, then write will be started from packet's beggining
 *              and retried until the packet is written to the specified end
 *              position, or an error other than EINTR is encountered.
 * @param end   Position in the packet the write should end at, NULL for
 *              position right after the end of the packet.
 *
 * @return Global return code.
 *         Can return TLOG_GRC_FROM(errno, EINTR), if ppos wasn't NULL, and
 *         writing was interrupted by a signal before anything was written.
 */
extern tlog_grc tlog_sink_write(struct tlog_sink *sink,
                                const struct tlog_pkt *pkt,
                                struct tlog_pkt_pos *ppos,
                                const struct tlog_pkt_pos *end);

/**
 * Cut a sink I/O - encode pending incomplete characters.
 *
 * @param sink  The sink to cut I/O for.
 *
 * @return Global return code.
 *         Can return TLOG_GRC_FROM(errno, EINTR), if writing was attempted
 *         and interrupted by a signal before anything was written.
 */
extern tlog_grc tlog_sink_cut(struct tlog_sink *sink);

/**
 * Flush data pending in a log sink.
 *
 * @param sink  The sink to flush.
 *
 * @return Global return code.
 *         Can return TLOG_GRC_FROM(errno, EINTR), if writing was attempted
 *         and interrupted by a signal before anything was written.
 */
extern tlog_grc tlog_sink_flush(struct tlog_sink *sink);

/**
 * Close I/O fd of a tty sink.
 *
 * @param sink   The sink to close an associated I/O file descriptor.
 * @param output True if sink output will be closed, false if sink
 *               input will be closed.
 */
extern void tlog_sink_io_close(struct tlog_sink *sink, bool output);

/**
 * Destroy (cleanup and free) a log sink.
 *
 * @param sink  Pointer to the sink to destroy, can be NULL.
 */
extern void tlog_sink_destroy(struct tlog_sink *sink);

#endif /* _TLOG_SINK_H */
