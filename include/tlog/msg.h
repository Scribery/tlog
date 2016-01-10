/**
 * @file
 * @brief JSON message parser.
 *
 * An object representing a single log message being parsed. Gets a struct
 * json_object on creation and provides an interface to read packets from.
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

#ifndef _TLOG_MSG_H
#define _TLOG_MSG_H

#include <stdbool.h>
#include <stdlib.h>
#include <json_object.h>
#include <tlog/grc.h>
#include <tlog/pkt.h>

/** Minimum I/O buffer size (longest UTF-8 character) */
#define TLOG_MSG_IO_SIZE_MIN    4

/**
 * Message.
 * NOTE: Members are named after JSON properties, where possible.
 */
struct tlog_msg {
    struct json_object *obj;            /**< The JSON object behind the
                                             message, NULL for a void
                                             message */
    const char         *host;           /**< Hostname */
    const char         *user;           /**< Username */
    unsigned int        session;        /**< Audit session ID */
    size_t              id;             /**< Message ID */
    struct timespec     pos;            /**< Position timestamp */

    const char         *timing_ptr;     /**< Timing string position */

    const char         *in_txt_ptr;     /**< Input text string position */
    size_t              in_txt_len;     /**< Input text remaining length */

    struct json_object *in_bin_obj;     /**< Input binary array object */
    int                 in_bin_pos;     /**< Input binary array position */

    const char         *out_txt_ptr;    /**< Output text string position */
    size_t              out_txt_len;    /**< Output text remaining length */

    struct json_object *out_bin_obj;    /**< Output binary array object */
    int                 out_bin_pos;    /**< Output binary array position */

    bool                output;         /**< True if currently processing an
                                             output timing entry */
    bool                binary;         /**< True if currently processing a
                                             binary timing entry */
    size_t              rem;            /**< Timing entry's remaining run
                                             length (bytes/characters) */

    const char            **ptxt_ptr;   /**< Current text string position */
    size_t                 *ptxt_len;   /**< Current text remaining length */

    struct json_object     *bin_obj;    /**< Current binary array object */
    int                    *pbin_pos;   /**< Current binary array position */
};

/**
 * Initialize a message.
 *
 * @param msg   The message to initialize.
 * @param obj   The object to parse, or NULL for void message.
 *              The object's reference count will be increased in case of
 *              success.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_msg_init(struct tlog_msg *msg,
                              struct json_object *obj);

/**
 * Check if a message is valid.
 *
 * @param msg   The message to check.
 *
 * @return True if the message is valid, false otherwise.
 */
extern bool tlog_msg_is_valid(const struct tlog_msg *msg);

/**
 * Check if a message is void (i.e. contains no data).
 *
 * @param msg   The message to check.
 *
 * @return True if the message is void, false otherwise.
 */
extern bool tlog_msg_is_void(const struct tlog_msg *msg);

/**
 * Read a packet from the message.
 *
 * @param msg       The message to read a packet from.
 * @param pkt       The packet to read into, must be void, will be void if
 *                  there are no more packets in the message.
 * @param io_buf    Pointer to buffer for writing I/O data to, which will be
 *                  referred to from the I/O packets as "not owned".
 * @param io_size   Size of the I/O buffer io_buf.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_msg_read(struct tlog_msg *msg, struct tlog_pkt *pkt,
                              uint8_t *io_buf, size_t io_size);

/**
 * Cleanup a message, "putting" down the JSON object and voiding the message.
 * The referenced object's reference count will be decreased.
 * Can be called repeatedly with no additional effect.
 *
 * @param msg   The message to cleanup.
 */
extern void tlog_msg_cleanup(struct tlog_msg *msg);

#endif /* _TLOG_MSG_H */
