/**
 * @file
 * @brief Terminal data sink type.
 *
 * A structure describing a terminal data sink implementation.
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

#ifndef _TLOG_SINK_TYPE_H
#define _TLOG_SINK_TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <tlog/grc.h>
#include <tlog/pkt.h>

/* Forward declaration */
struct tlog_sink;

/**
 * Init function prototype.
 *
 * @param sink  The sink to initialize, must be cleared to zero.
 * @param ap    Argument list.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_sink_type_init_fn)(struct tlog_sink *sink,
                                           va_list ap);

/**
 * Validation function prototype.
 *
 * @param sink    The sink to operate on.
 *
 * @return True if the sink is valid, false otherwise
 */
typedef bool (*tlog_sink_type_is_valid_fn)(const struct tlog_sink *sink);

/**
 * Packet writing function prototype.
 *
 * @param sink  Pointer to the sink to write the packet to.
 * @param pkt   The packet to write.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_sink_type_write_fn)(struct tlog_sink *sink,
                                            const struct tlog_pkt *pkt);

/**
 * I/O-cutting function prototype.
 *
 * @param sink  The sink to cut I/O for.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_sink_type_cut_fn)(struct tlog_sink *sink);

/**
 * Data-flushing function prototype.
 *
 * @param sink  The sink to flush.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_sink_type_flush_fn)(struct tlog_sink *sink);

/**
 * Cleanup function prototype.
 *
 * @param sink  The sink to cleanup.
 */
typedef void (*tlog_sink_type_cleanup_fn)(struct tlog_sink *sink);

/** Sink type */
struct tlog_sink_type {
    size_t                      size;       /**< Instance size */
    tlog_sink_type_init_fn      init;       /**< Init function */
    tlog_sink_type_is_valid_fn  is_valid;   /**< Validation function */
    tlog_sink_type_write_fn     write;      /**< Writing function */
    tlog_sink_type_cut_fn       cut;        /**< I/O-cutting function */
    tlog_sink_type_flush_fn     flush;      /**< Flushing function */
    tlog_sink_type_cleanup_fn   cleanup;    /**< Cleanup function */
};

/**
 * Check if a sink type is valid.
 *
 * @param type  The sink type to check.
 *
 * @return True if the sink type is valid.
 */
static inline bool
tlog_sink_type_is_valid(const struct tlog_sink_type *type)
{
    return type != NULL &&
           type->size != 0 &&
           type->init != NULL &&
           type->write != NULL;
}

#endif /* _TLOG_SINK_TYPE_H */
