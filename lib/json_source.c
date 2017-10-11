/*
 * JSON source.
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

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <tlog/timespec.h>
#include <tlog/json_msg.h>
#include <tlog/json_source.h>

bool
tlog_json_source_params_is_valid(const struct tlog_json_source_params *params)
{
    return params != NULL &&
           tlog_json_reader_is_valid(params->reader) &&
           params->io_size >= TLOG_JSON_SOURCE_IO_SIZE_MIN;
}

/** JSON source instance */
struct tlog_json_source {
    struct tlog_source          source; /**< Abstract source instance */

    struct tlog_json_reader    *reader; /**< JSON log message reader */

    bool                reader_owned;   /**< True if reader is owned */

    char               *hostname;       /**< Hostname to filter messages by,
                                             NULL for unfiltered */
    bool                filter_recording;   /**< True if messages should be
                                                 filtered by recording ID */
    char               *recording;      /**< Recording ID to filter messages
                                             by, NULL for no field */
    char               *username;       /**< Username to filter messages by,
                                             NULL for unfiltered */
    char               *terminal;       /**< Terminal type to require in
                                             messages, NULL for any */
    unsigned int        session_id;     /**< Session ID to filter messages by,
                                             NULL for unfiltered */

    bool                lax;            /**< Ignore missing messages, i.e.
                                             message ID jumps, if true */
    bool                got_msg;        /**< Read at least one message */
    size_t              last_msg_id;    /**< Last message ID */
    bool                got_pkt;        /**< Read at least one packet */
    struct timespec     last_pkt_ts;    /**< Last packet timestamp */
    bool                got_window;     /**< Read at least one window */
    unsigned short int  last_width;     /**< Last window's width */
    unsigned short int  last_height;    /**< Last window's height */

    struct tlog_json_msg    msg;        /**< Message parsing state */

    uint8_t            *io_buf;         /**< I/O data buffer used in packets */
    size_t              io_size;        /**< I/O data buffer length */
};

static bool
tlog_json_source_is_valid(const struct tlog_source *source)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    return json_source != NULL &&
           tlog_json_reader_is_valid(json_source->reader) &&
           json_source->io_buf != NULL &&
           json_source->io_size >= TLOG_JSON_SOURCE_IO_SIZE_MIN;
}

static void
tlog_json_source_cleanup(struct tlog_source *source)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    assert(json_source != NULL);
    tlog_json_msg_cleanup(&json_source->msg);
    free(json_source->hostname);
    json_source->hostname = NULL;
    free(json_source->recording);
    json_source->recording = NULL;
    free(json_source->username);
    json_source->username = NULL;
    free(json_source->io_buf);
    json_source->io_buf = NULL;
    if (json_source->reader_owned) {
        tlog_json_reader_destroy(json_source->reader);
        json_source->reader_owned = false;
    }
}

static tlog_grc
tlog_json_source_init(struct tlog_source *source, va_list ap)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    tlog_grc grc;
    const struct tlog_json_source_params *params =
                        va_arg(ap, const struct tlog_json_source_params *);

    assert(tlog_json_source_params_is_valid(params));

    if (params->hostname != NULL) {
        json_source->hostname = strdup(params->hostname);
        if (json_source->hostname == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
    }
    json_source->filter_recording = params->filter_recording;
    if (json_source->filter_recording && params->recording != NULL) {
        json_source->recording = strdup(params->recording);
        if (json_source->recording == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
    }
    if (params->username != NULL) {
        json_source->username = strdup(params->username);
        if (json_source->username == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
    }
    if (params->terminal != NULL) {
        json_source->terminal = strdup(params->terminal);
        if (json_source->terminal == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
    }
    json_source->session_id = params->session_id;
    json_source->lax = params->lax;

    tlog_json_msg_init(&json_source->msg, NULL);

    json_source->io_size = params->io_size;
    json_source->io_buf = malloc(params->io_size);
    if (json_source->io_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    json_source->reader = params->reader;
    json_source->reader_owned = params->reader_owned;

    return TLOG_RC_OK;
error:
    tlog_json_source_cleanup(source);
    return grc;
}

static size_t
tlog_json_source_loc_get(const struct tlog_source *source)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    return tlog_json_reader_loc_get(json_source->reader);
}

static char *
tlog_json_source_loc_fmt(const struct tlog_source *source, size_t loc)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    return tlog_json_reader_loc_fmt(json_source->reader, loc);
}

/**
 * Read a matching JSON message from the source's reader.
 *
 * @param json_source    The source to read message for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_json_source_read_msg(struct tlog_source *source)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    tlog_grc grc;
    struct json_object *obj;

    assert(tlog_json_source_is_valid(source));
    assert(tlog_json_msg_is_void(&json_source->msg));

    for (; ; tlog_json_msg_cleanup(&json_source->msg)) {
        grc = tlog_json_reader_read(json_source->reader, &obj);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
        if (obj == NULL) {
            return TLOG_RC_OK;
        }

        grc = tlog_json_msg_init(&json_source->msg, obj);
        json_object_put(obj);
        if (grc != TLOG_RC_OK) {
            return grc;
        }

        if (json_source->hostname != NULL &&
            strcmp(json_source->msg.host, json_source->hostname) != 0) {
            continue;
        }
        if (json_source->filter_recording &&
            ((json_source->recording != NULL) !=
                (json_source->msg.rec != NULL) ||
             (json_source->recording != NULL &&
              strcmp(json_source->msg.rec, json_source->recording) != 0))) {
            continue;
        }
        if (json_source->username != NULL &&
            strcmp(json_source->msg.user, json_source->username) != 0) {
            continue;
        }
        if (json_source->terminal != NULL &&
            strcmp(json_source->msg.term, json_source->terminal) != 0) {
            return TLOG_RC_JSON_SOURCE_TERMINAL_MISMATCH;
        }
        if (json_source->session_id != 0 &&
            json_source->msg.session != json_source->session_id) {
            continue;
        }

        return TLOG_RC_OK;
    }
}

static tlog_grc
tlog_json_source_read(struct tlog_source *source, struct tlog_pkt *pkt)
{
    struct tlog_json_source *json_source =
                                (struct tlog_json_source *)source;
    tlog_grc grc;
    struct tlog_json_msg *msg;

    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));

    msg = &json_source->msg;

    while (true) {
        if (tlog_json_msg_is_void(msg)) {
            grc = tlog_json_source_read_msg(source);
            if (grc != TLOG_RC_OK) {
                return grc;
            }
            if (tlog_json_msg_is_void(msg)) {
                return TLOG_RC_OK;
            }
            if (json_source->got_msg) {
                if (json_source->lax
                        ? (msg->id <= json_source->last_msg_id)
                        : (msg->id != (json_source->last_msg_id + 1))) {
                    tlog_json_msg_cleanup(msg);
                    return TLOG_RC_JSON_SOURCE_MSG_ID_OUT_OF_ORDER;
                }
            } else {
                json_source->got_msg = true;
            }
            json_source->last_msg_id = msg->id;
        }

        grc = tlog_json_msg_read(msg, pkt,
                                 json_source->io_buf, json_source->io_size);
        if (grc != TLOG_RC_OK) {
            tlog_json_msg_cleanup(msg);
            return grc;
        }

        if (tlog_pkt_is_void(pkt)) {
            tlog_json_msg_cleanup(msg);
        } else {
            if (json_source->got_pkt) {
                if (tlog_timespec_cmp(&pkt->timestamp,
                                      &json_source->last_pkt_ts) < 0) {
                    tlog_pkt_cleanup(pkt);
                    tlog_json_msg_cleanup(msg);
                    return TLOG_RC_JSON_SOURCE_PKT_TS_OUT_OF_ORDER;
                }
            } else {
                json_source->got_pkt = true;
            }

            json_source->last_pkt_ts = pkt->timestamp;

            /* Don't return the same window */
            if (pkt->type == TLOG_PKT_TYPE_WINDOW) {
                if (json_source->got_window) {
                    if (pkt->data.window.width == json_source->last_width &&
                        pkt->data.window.height == json_source->last_height) {
                        tlog_pkt_cleanup(pkt);
                        continue;
                    }
                } else {
                    json_source->got_window = true;
                }
                json_source->last_width = pkt->data.window.width;
                json_source->last_height = pkt->data.window.height;
            }

            return TLOG_RC_OK;
        }
    }
}

const struct tlog_source_type tlog_json_source_type = {
    .size       = sizeof(struct tlog_json_source),
    .init       = tlog_json_source_init,
    .cleanup    = tlog_json_source_cleanup,
    .is_valid   = tlog_json_source_is_valid,
    .read       = tlog_json_source_read,
    .loc_get    = tlog_json_source_loc_get,
    .loc_fmt    = tlog_json_source_loc_fmt,
};
