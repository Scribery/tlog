/**
 * @file
 * @brief JSON encoder dispatcher interface.
 *
 * Dispatcher is a proxy object - a narrow interface used to decouple chunks
 * and streams, so that streams don't require a full chunk implementation and
 * could be tested separately.
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

#ifndef _TLOG_DISPATCHER_H
#define _TLOG_DISPATCHER_H

#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <tlog/trx.h>

/** Forward declaration of dispatcher */
struct tlog_dispatcher;

/**
 * Time-advancing function prototype.
 *
 * @param trx           The transaction to act within.
 * @param dispatcher    The dispatcher to advance time for.
 * @param ts            The time to advance to, must be equal or greater than
 *                      the previously advanced to.
 *
 * @return True if there was space to record the advanced time, false
 *         otherwise.
 */
typedef bool (*tlog_dispatcher_advance_fn)(tlog_trx_state trx,
                                           struct tlog_dispatcher *dispatcher,
                                           const struct timespec *ts);

/**
 * Space-reservation function prototype.
 *
 * @param dispatcher    The dispatcher to reserve the space from.
 * @param len           The amount of space to reserve.
 *
 * @return True if there was enough space, false otherwise.
 */
typedef bool (*tlog_dispatcher_reserve_fn)(struct tlog_dispatcher *dispatcher,
                                           size_t len);

/**
 * Metadata writing function prototype. Prints into the reserved space.
 *
 * @param dispatcher    The dispatcher to print to.
 * @param ptr           Pointer to the data to write.
 * @param len           Length of the data to write.
 */
typedef void (*tlog_dispatcher_write_fn)(struct tlog_dispatcher    *dispatcher,
                                         const uint8_t             *ptr,
                                         size_t                     len);

/** Dispatcher */
struct tlog_dispatcher {
    tlog_dispatcher_advance_fn      advance;        /**< Time-advancing function */
    tlog_dispatcher_reserve_fn      reserve;        /**< Space-reservation function */
    tlog_dispatcher_write_fn        write;          /**< Metadata writing function */
    struct tlog_trx_iface           trx_iface;      /**< Transaction interface */
    void                           *container;              /**< Container */
    const struct tlog_trx_iface    *container_trx_iface;    /**< Container's
                                                                 transaction
                                                                 interface */
};

/**
 * Check if a dispatcher is valid.
 *
 * @param dispatcher    The dispatcher to check.
 *
 * @return True if the dispatcher is valid, false otherwise.
 */
static inline bool
tlog_dispatcher_is_valid(const struct tlog_dispatcher *dispatcher)
{
    return dispatcher != NULL &&
           dispatcher->advance != NULL &&
           dispatcher->reserve != NULL &&
           dispatcher->write != NULL &&
           dispatcher->container != NULL &&
           dispatcher->container_trx_iface != NULL;
}

/**
 * Initialize a dispatcher.
 *
 * @param dispatcher            The dispatcher to initialize.
 * @param advance               The time-advancing function.
 * @param reserve               The space-reservation function.
 * @param write                 The metadata writing function.
 * @param container             Container pointer.
 * @param container_trx_iface   The container's transaction interface to use.
 */
extern void tlog_dispatcher_init(struct tlog_dispatcher *dispatcher,
                                 tlog_dispatcher_advance_fn advance,
                                 tlog_dispatcher_reserve_fn reserve,
                                 tlog_dispatcher_write_fn write,
                                 void *container,
                                 const struct tlog_trx_iface
                                                *container_trx_iface);

/**
 * Advance dispatcher time.
 *
 * @param trx           The transaction to act within.
 * @param dispatcher    The dispatcher to advance time for.
 * @param ts            The time to advance to, must be equal or greater than
 *                      the previously advanced to.
 *
 * @return True if there was space to record the advanced time, false
 *         otherwise.
 */
static inline bool
tlog_dispatcher_advance(tlog_trx_state trx,
                        struct tlog_dispatcher *dispatcher,
                        const struct timespec *ts)
{
    assert(tlog_dispatcher_is_valid(dispatcher));
    assert(ts != NULL);
    return dispatcher->advance(trx, dispatcher, ts);
}

/**
 * Reserve space from the dispatcher.
 *
 * @param dispatcher    The dispatcher to reserve the space from.
 * @param len           The amount of space to reserve.
 *
 * @return True if there was enough space, false otherwise.
 */
static inline bool
tlog_dispatcher_reserve(struct tlog_dispatcher *dispatcher,
                        size_t len)
{
    assert(tlog_dispatcher_is_valid(dispatcher));
    return dispatcher->reserve(dispatcher, len);
}

/**
 * Write into the metadata space reserved from the dispatcher.
 *
 * @param dispatcher    The dispatcher to print to.
 * @param ptr           Pointer to the data to write.
 * @param len           Length of the data to write.
 */
static inline void
tlog_dispatcher_write(struct tlog_dispatcher *dispatcher,
                      const uint8_t *ptr, size_t len)
{
    assert(tlog_dispatcher_is_valid(dispatcher));
    assert(ptr != NULL || len == 0);
    dispatcher->write(dispatcher, ptr, len);
}

#endif /* _TLOG_DISPATCHER_H */
