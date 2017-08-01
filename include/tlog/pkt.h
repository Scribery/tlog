/**
 * @file
 * @brief Packet.
 *
 * A packet stores any possible terminal data: window size changes or I/O.
 * A "void" packet is a packet not containing any data. Such packet can be
 * initialized to contain any data.
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

#ifndef _TLOG_PKT_H
#define _TLOG_PKT_H

#include <assert.h>
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
    return type < TLOG_PKT_TYPE_NUM;
}

/**
 * Convert packet type to a constant string.
 *
 * @param type  The type to convert.
 *
 * @return Type string.
 */
extern const char *tlog_pkt_type_to_str(enum tlog_pkt_type type);

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
#define TLOG_PKT_VOID \
    ((struct tlog_pkt){.type = TLOG_PKT_TYPE_VOID})

/** Window packet initializer */
#define TLOG_PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height) \
    ((struct tlog_pkt){                                     \
        .timestamp  = {_tv_sec, _tv_nsec},                  \
        .type       = TLOG_PKT_TYPE_WINDOW,                 \
        .data       = {                                     \
            .window = {                                     \
                .width  = _width,                           \
                .height = _height                           \
            }                                               \
        }                                                   \
    })

/** Constant buffer I/O packet initializer */
#define TLOG_PKT_IO(_tv_sec, _tv_nsec, _output, _buf, _len) \
    ((struct tlog_pkt){                                     \
        .timestamp  = {                                     \
                .tv_sec  = _tv_sec,                         \
                .tv_nsec = _tv_nsec                         \
        },                                                  \
        .type       = TLOG_PKT_TYPE_IO,                     \
        .data       = {                                     \
            .io = {                                         \
                .output     = _output,                      \
                .buf        = (uint8_t *)_buf,              \
                .buf_owned  = false,                        \
                .len        = _len                          \
            }                                               \
        }                                                   \
    })

/** Constant string I/O packet initializer */
#define TLOG_PKT_IO_STR(_tv_sec, _tv_nsec, _output, _str) \
    ((struct tlog_pkt){                                     \
        .timestamp  = {_tv_sec, _tv_nsec},                  \
        .type       = TLOG_PKT_TYPE_IO,                     \
        .data       = {                                     \
            .io = {                                         \
                .output     = _output,                      \
                .buf        = (uint8_t *)_str,              \
                .buf_owned  = false,                        \
                .len        = strlen(_str)                  \
            }                                               \
        }                                                   \
    })

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
 * @param buf       I/O buffer pointer.
 * @param buf_owned True if the I/O buffer should be freed on packet cleanup.
 * @param len       I/O buffer length.
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
 * Check if contents of one packet is equal to the contents of another one.
 *
 * @param a     First packet to compare.
 * @param b     Second packet to compare.
 *
 * @return True if packets contents are equal, false otherwise.
 */
extern bool tlog_pkt_is_equal(const struct tlog_pkt *a,
                              const struct tlog_pkt *b);

/**
 * Cleanup a packet, freeing owned data if any.
 * Voids the packet. Can be called repeatedly.
 *
 * @param pkt   The packet to cleanup.
 */
extern void tlog_pkt_cleanup(struct tlog_pkt *pkt);

/** Position inside a packet */
struct tlog_pkt_pos {
    enum tlog_pkt_type  type;   /**< Type of packet */
    size_t              val;    /**< Position value */
};

/** Void (zero) position initializer */
#define TLOG_PKT_POS_VOID \
    ((struct tlog_pkt_pos){         \
        .type = TLOG_PKT_TYPE_VOID, \
        .val = 0                    \
    })

/**
 * Check if a packet position is valid.
 *
 * @param pos   The position to check.
 *
 * @return True if the position is valid, false otherwise.
 */
extern bool tlog_pkt_pos_is_valid(const struct tlog_pkt_pos *pos);

/**
 * Check if a packet position is compatible with a particular packet.
 *
 * @param pos   The position to check.
 * @param pkt   The packet to check against.
 *
 * @return True if the position is compatible with the specified packet, false
 *         otherwise.
 */
static inline bool
tlog_pkt_pos_is_compatible(const struct tlog_pkt_pos *pos,
                           const struct tlog_pkt *pkt)
{
    assert(tlog_pkt_pos_is_valid(pos));
    assert(tlog_pkt_is_valid(pkt));
    return pos->type == TLOG_PKT_TYPE_VOID ||
           pos->type == pkt->type;
}

/**
 * Check if a packet position is "reachable" within specified compatible packet.
 * I.e. that this position is either in, or right after the packet's data.
 *
 * @param pos   The position to check.
 * @param pkt   The packet to check against, must be compatible with position.
 *
 * @return True if the position is reachable within the specified packet, false
 *         otherwise.
 */
extern bool tlog_pkt_pos_is_reachable(const struct tlog_pkt_pos *pos,
                                      const struct tlog_pkt *pkt);

/**
 * Check if a packet position is inside a compatible packet.
 *
 * @param pos   The position to check.
 * @param pkt   The packet to check against, must be compatible with position.
 *
 * @return True if the position is inside the packet, false otherwise.
 */
extern bool tlog_pkt_pos_is_in(const struct tlog_pkt_pos *pos,
                               const struct tlog_pkt *pkt);

/**
 * Check if a packet position is at or past the end of a compatible packet.
 *
 * @param pos   The position to check.
 * @param pkt   The packet to check against, must be compatible with position.
 *
 * @return True if the position is at or past the end of the packet, false
 *         otherwise.
 */
static inline bool
tlog_pkt_pos_is_past(const struct tlog_pkt_pos *pos,
                     const struct tlog_pkt *pkt)
{
    assert(tlog_pkt_pos_is_valid(pos));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_pos_is_compatible(pos, pkt));
    return !tlog_pkt_pos_is_in(pos, pkt);
}

/**
 * Check if a packet position is comparable to another.
 *
 * @param a     Position to check.
 * @param b     Position to check against.
 *
 * @return True if the positions are comparable, false otherwise.
 */
static inline bool
tlog_pkt_pos_is_comparable(const struct tlog_pkt_pos *a,
                           const struct tlog_pkt_pos *b)
{
    assert(tlog_pkt_pos_is_valid(a));
    assert(tlog_pkt_pos_is_valid(b));
    return a->type == b->type ||
           a->type == TLOG_PKT_TYPE_VOID ||
           b->type == TLOG_PKT_TYPE_VOID;
}

/**
 * Compare two packet positions of the same type.
 *
 * @param a     First position to compare.
 * @param b     Second position to compare.
 *
 * @return Comparison result, one of:
 *          -1  - a < b
 *           0  - a == b
 *           1  - a > b
 */
extern int tlog_pkt_pos_cmp(const struct tlog_pkt_pos *a,
                            const struct tlog_pkt_pos *b);

/**
 * Add specified amount to packet position within specified compatible packet.
 * The result cannot be unreachable.
 *
 * @param pos   The position to modify.
 * @param pkt   The packet to act within.
 * @param off   The value to add.
 */
extern void tlog_pkt_pos_move(struct tlog_pkt_pos *pos,
                              const struct tlog_pkt *pkt,
                              ssize_t off);

/**
 * Move packet position past the specified compatible packet's contents.
 *
 * @param pos   The position to modify.
 * @param pkt   The packet to act within.
 */
extern void tlog_pkt_pos_move_past(struct tlog_pkt_pos *pos,
                                   const struct tlog_pkt *pkt);

#endif /* _TLOG_PKT_H */
