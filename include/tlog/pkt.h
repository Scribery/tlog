/*
 * Tlog packet.
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

#ifndef _TLOG_PKT_H
#define _TLOG_PKT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/** Packet type */
enum tlog_pkt_type {
    TLOG_PKT_TYPE_VOID,     /**< Void (typeless) packet */
    TLOG_PKT_TYPE_WINDOW,   /**< Window size change */
    TLOG_PKT_TYPE_IO,       /**< I/O data */
    TLOG_PKT_TYPE_NUM       /**< Number of types (not a type itself) */
};

/**
 * Check if a packet type is valid.
 *
 * @param type  The packet type to check.
 *
 * @return True if the packet type is valid, false otherwise.
 */
static inline bool
tlog_pkt_type_is_valid(enum tlog_pkt_type type)
{
    return type >= 0 && type < TLOG_PKT_TYPE_NUM;
}

/** Window change data */
struct tlog_pkt_data_window {
    unsigned short int  width;  /**< New window width */
    unsigned short int  height; /**< New window height */
};

/** I/O data */
struct tlog_pkt_data_io {
    bool        output;     /**< True if this is output data,
                                 false if input data */
    uint8_t    *buf;        /**< I/O data buffer */
    bool        buf_owned;  /**< True if the buffer should be freed
                                 on packet cleanup, false otherwise */
    size_t      len;        /**< I/O data length */
};

/** Packet */
struct tlog_pkt {
    struct timespec     timestamp;      /**< Timestamp */
    enum tlog_pkt_type  type;           /**< Packet type */
    union {
        struct tlog_pkt_data_window     window; /**< Window change data */
        struct tlog_pkt_data_io         io;     /**< I/O data */
    } data;                             /**< Type-specific data */
};

/** Void packet initializer */
#define TLOG_PKT_VOID   (struct tlog_pkt){.type = TLOG_PKT_TYPE_VOID}

/**
 * Initialize a void packet.
 *
 * @param pkt   The packet to initialize.
 */
extern void tlog_pkt_init(struct tlog_pkt *pkt);

/**
 * Initialize a window change packet.
 *
 * @param pkt       The packet to initialize.
 * @param timestamp Timestamp of the window change.
 * @param width     Window width in characters.
 * @param height    Window height in characters.
 */
extern void tlog_pkt_init_window(struct tlog_pkt *pkt,
                                 const struct timespec *timestamp,
                                 unsigned short int width,
                                 unsigned short int height);

/**
 * Initialize an I/O data packet.
 *
 * @param pkt       The packet to initialize.
 * @param timestamp Timestamp of the I/O arrival.
 * @param output    True if writing output, false if input.
 * @param buf       Input buffer pointer.
 * @param buf_owned True if input buffer should be freed on packet cleanup.
 * @param len       Input buffer length.
 */
extern void tlog_pkt_init_io(struct tlog_pkt *pkt,
                             const struct timespec *timestamp,
                             bool output,
                             uint8_t *buf,
                             bool buf_owned,
                             size_t len);

/**
 * Check if a packet is valid.
 *
 * @param pkt   The packet to check.
 *
 * @return True if the packet is valid, false otherwise.
 */
extern bool tlog_pkt_is_valid(const struct tlog_pkt *pkt);

/**
 * Check if a packet is void.
 *
 * @param pkt   The packet to check.
 *
 * @return True if the packet is void, false otherwise.
 */
extern bool tlog_pkt_is_void(const struct tlog_pkt *pkt);

/**
 * Cleanup a packet, freeing owned data if any.
 * Voids the packet. Can be called repeatedly.
 *
 * @param pkt   The packet to cleanup.
 */
extern void tlog_pkt_cleanup(struct tlog_pkt *pkt);

#endif /* _TLOG_PKT_H */
