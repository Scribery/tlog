/*
 * JSON message parser.
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
#include <limits.h>
#include <stdio.h>
#include <tlog/rc.h>
#include <tlog/timespec.h>
#include <tlog/delay.h>
#include <tlog/misc.h>
#include <tlog/json_msg.h>

bool
tlog_json_msg_is_valid(const struct tlog_json_msg *msg)
{
    if (msg == NULL) {
        return false;
    }

    if (msg->obj == NULL) {
        return true;
    }

    return msg->host != NULL &&
           msg->user != NULL &&
           msg->term != NULL &&
           msg->session > 0 &&
           msg->timing_ptr != NULL &&
           msg->in_txt_ptr != NULL &&
           msg->in_bin_obj != NULL &&
           msg->in_bin_pos >= 0 &&
           msg->out_txt_ptr != NULL &&
           msg->out_bin_obj != NULL &&
           msg->out_bin_pos >= 0;
}

bool
tlog_json_msg_is_void(const struct tlog_json_msg *msg)
{
    assert(tlog_json_msg_is_valid(msg));
    return msg->obj == NULL;
}

tlog_grc
tlog_json_msg_init(struct tlog_json_msg *msg, struct json_object *obj)
{
    struct json_object *o;
    int rc;
    const char *str;
    int end;
    int64_t session;
    int64_t id;
    int64_t pos;

    assert(msg != NULL);

    memset(msg, 0, sizeof(*msg));

    if (obj == NULL) {
        assert(tlog_json_msg_is_valid(msg));
        return true;
    }

#define GET_FIELD(_name_token, _type_token) \
    do {                                                            \
        if (!json_object_object_get_ex(obj, #_name_token, &o)) {    \
            return TLOG_RC_JSON_MSG_FIELD_MISSING;                  \
        }                                                           \
        if (json_object_get_type(o) != json_type_##_type_token) {   \
            return TLOG_RC_JSON_MSG_FIELD_INVALID_TYPE;             \
        }                                                           \
    } while (0)

#define GET_FIELD_ALT_TYPE2(_name_token, _type_token1, _type_token2) \
    do {                                                                \
        if (!json_object_object_get_ex(obj, #_name_token, &o)) {        \
            return TLOG_RC_JSON_MSG_FIELD_MISSING;                      \
        }                                                               \
        if (json_object_get_type(o) != json_type_##_type_token1 &&      \
            json_object_get_type(o) != json_type_##_type_token2) {      \
            return TLOG_RC_JSON_MSG_FIELD_INVALID_TYPE;                 \
        }                                                               \
    } while (0)

#define GET_OPTIONAL_FIELD(_name_token, _type_token) \
    do {                                                            \
        if (json_object_object_get_ex(obj, #_name_token, &o) &&     \
            json_object_get_type(o) != json_type_##_type_token) {   \
            return TLOG_RC_JSON_MSG_FIELD_INVALID_TYPE;             \
        }                                                           \
    } while (0)

    GET_FIELD_ALT_TYPE2(ver, string, int);
    /* NOTE: Converting number to string can fail */
    str = json_object_get_string(o);
    if (str == NULL) {
        return TLOG_GRC_ERRNO;
    }
    end = 0;
    rc = sscanf(str, "%u%n.%u%n",
                &msg->ver_major, &end, &msg->ver_minor, &end);
    if (rc < 1 || str[end] != '\0' || msg->ver_major > 2) {
        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_VER;
    }

    GET_FIELD(host, string);
    msg->host = json_object_get_string(o);

    GET_OPTIONAL_FIELD(rec, string);
    msg->rec = json_object_get_string(o);

    GET_FIELD(user, string);
    msg->user = json_object_get_string(o);

    GET_FIELD(term, string);
    msg->term = json_object_get_string(o);

    GET_FIELD(session, int);
    session = json_object_get_int64(o);
    if (session < 1 || session > UINT_MAX) {
        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_SESSION;
    }
    msg->session = (unsigned int)session;

    GET_FIELD(id, int);
    id = json_object_get_int64(o);
    if (id < 0
#if INT64_MAX > SIZE_MAX
        || id > (int64_t)SIZE_MAX
#endif
    ) {
        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_ID;
    }
    msg->id = (size_t)id;

    GET_FIELD(pos, int);
    pos = json_object_get_int64(o);
    if (pos < 0 || pos > TLOG_DELAY_MAX_MS_NUM) {
        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_POS;
    }
    msg->pos.tv_sec = pos / 1000;
    msg->pos.tv_nsec = pos % 1000 * 1000000;

    GET_FIELD(timing, string);
    msg->timing_ptr = json_object_get_string(o);

    GET_FIELD(in_txt, string);
    msg->in_txt_ptr = json_object_get_string(o);
    msg->in_txt_len = (size_t)json_object_get_string_len(o);

    GET_FIELD(in_bin, array);
    msg->in_bin_obj = o;
    msg->in_bin_pos = 0;

    GET_FIELD(out_txt, string);
    msg->out_txt_ptr = json_object_get_string(o);
    msg->out_txt_len = (size_t)json_object_get_string_len(o);

    GET_FIELD(out_bin, array);
    msg->out_bin_obj = o;
    msg->out_bin_pos = 0;

#undef GET_OPTIONAL_FIELD
#undef GET_FIELD_ALT_TYPE2
#undef GET_FIELD

    msg->obj = json_object_get(obj);
    assert(tlog_json_msg_is_valid(msg));
    return TLOG_RC_OK;
}

/**
 * Retrieve length of a UTF-8 character.
 *
 * @param b     The first byte of the character to retrieve length of.
 *
 * @return The character length in bytes, or zero if character is invalid.
 */
static size_t
tlog_json_msg_utf8_len(uint8_t b)
{
    if ((b & 0x80) == 0) {
        return 1;
    } else if ((b & 0xe0) == 0xc0) {
        return 2;
    } else if ((b & 0xf0) == 0xe0) {
        return 3;
    } else if ((b & 0xf8) == 0xf0) {
        return 4;
    } else {
        return 0;
    }
};

tlog_grc
tlog_json_msg_read(struct tlog_json_msg *msg, struct tlog_pkt *pkt,
                   uint8_t *io_buf, size_t io_size)
{
    const char                 *timing_ptr;
    size_t                      io_len = 0;
    bool                        io_full = false;
    bool                        pkt_output;

    assert(tlog_json_msg_is_valid(msg));
    assert(!tlog_json_msg_is_void(msg));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));
    assert(io_buf != NULL);
    assert(io_size >= TLOG_JSON_MSG_IO_SIZE_MIN);

    /* Until the I/O buffer (io_buf/io_size) is full */
    do {
        /*
         * Read next timing record if the current one is spent
         */
        if (msg->rem == 0) {
            char            type_buf[2];
            char            type;
            int             read;
            uint64_t        first_val;
            uint64_t        second_val;
            struct timespec delay;

            /* Skip leading whitespace */
            while (true) {
                switch (*msg->timing_ptr) {
                case ' ':
                case '\f':
                case '\n':
                case '\r':
                case '\t':
                case '\v':
                    msg->timing_ptr++;
                    continue;
                }
                break;
            }

            /* Modify timing pointer on the side to be able to rollback */
            timing_ptr = msg->timing_ptr;

            /* If reached the end of timing and so the end of data */
            if (*timing_ptr == 0) {
                /* Return whatever we have */
                break;
            }

            if (sscanf(timing_ptr, "%1[][><+=]%" SCNu64 "%n",
                       type_buf, &first_val, &read) < 2) {
                return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
            }
            type = *type_buf;
            timing_ptr += read;

            if (type == '[' || type == ']') {
                if (sscanf(timing_ptr, "/%" SCNu64 "%n", &second_val, &read) < 1) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                timing_ptr += read;
            } else if (type == '=') {
                if (sscanf(timing_ptr, "x%" SCNu64 "%n", &second_val, &read) < 1) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                timing_ptr += read;
            } else {
                second_val = 0;
            }

            /* If it is a delay record */
            if (type == '+') {
                if (first_val != 0) {
                    if (first_val > TLOG_DELAY_MAX_MS_NUM) {
                        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                    }
                    /* If there was I/O already */
                    if (io_len > 0) {
                        /*
                         * We gotta return the old pos packet and re-read
                         * delay record next time
                         */
                        break;
                    }
                    delay.tv_sec = first_val / 1000;
                    delay.tv_nsec = first_val % 1000 * 1000000;
                    tlog_timespec_add(&msg->pos, &delay, &msg->pos);
                }
                /* Timing record consumed */
                msg->timing_ptr = timing_ptr;
                /* Read next timing record - no I/O from this one */
                continue;
            /* If it is a window record */
            } else if (type == '=') {
                /* If there was I/O already */
                if (io_len > 0) {
                    /*
                     * We gotta return the I/O packet and re-read
                     * window record next time
                     */
                    break;
                }
                /* Check extents */
                if (first_val > USHRT_MAX || second_val > USHRT_MAX) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                /* Return window packet */
                tlog_pkt_init_window(pkt, &msg->pos,
                                     (unsigned short int)first_val,
                                     (unsigned short int)second_val);
                /* Timing record consumed */
                msg->timing_ptr = timing_ptr;
                return TLOG_RC_OK;
            /* If it is a text input record */
            } else if (type == '<') {
                if (first_val > SIZE_MAX) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                msg->output = false;
                msg->binary = false;
                msg->rem = first_val;
                msg->ptxt_ptr = &msg->in_txt_ptr;
                msg->ptxt_len = &msg->in_txt_len;
            /* If it is a binary input record */
            } else if (type == '[') {
                if (first_val > SIZE_MAX || second_val > SIZE_MAX) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                msg->output = false;
                msg->binary = true;
                msg->rem = second_val;
                msg->ptxt_ptr = &msg->in_txt_ptr;
                msg->ptxt_len = &msg->in_txt_len;
                msg->bin_obj = msg->in_bin_obj;
                msg->pbin_pos = &msg->in_bin_pos;
            /* If it is a text output record */
            } else if (type == '>') {
                if (first_val > SIZE_MAX) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                msg->output = true;
                msg->binary = false;
                msg->rem = first_val;
                msg->ptxt_ptr = &msg->out_txt_ptr;
                msg->ptxt_len = &msg->out_txt_len;
            /* If it is a binary output record */
            } else if (type == ']') {
                if (first_val > SIZE_MAX || second_val > SIZE_MAX) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
                }
                msg->output = true;
                msg->binary = true;
                msg->rem = second_val;
                msg->ptxt_ptr = &msg->out_txt_ptr;
                msg->ptxt_len = &msg->out_txt_len;
                msg->bin_obj = msg->out_bin_obj;
                msg->pbin_pos = &msg->out_bin_pos;
            } else {
                assert(false);
                return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING;
            }

            /* Timing record consumed */
            msg->timing_ptr = timing_ptr;

            if (msg->binary) {
                size_t l;
                /* Skip replacement characters */
                for (; first_val > 0; first_val--) {
                    /* If not enough text */
                    if (*msg->ptxt_len == 0) {
                        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT;
                    }
                    l = tlog_json_msg_utf8_len(*(uint8_t *)*msg->ptxt_ptr);
                    /* If found invalid UTF-8 character in text */
                    if (l == 0) {
                        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT;
                    }
                    /* If character crosses text boundary */
                    if (l > *msg->ptxt_len) {
                        return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT;
                    }
                    *msg->ptxt_len -= l;
                    *msg->ptxt_ptr += l;
                }
            }

            /* Ignore zero I/O */
            if (msg->rem == 0) {
                continue;
            }
        }

        /* Stop if the packet I/O is in different direction */
        if (io_len == 0) {
            pkt_output = msg->output;
        } else if (pkt_output != msg->output) {
            /* Return whatever we have */
            break;
        }

        /*
         * Append (a piece of) I/O to the output buffer
         */
        if (msg->binary) {
            struct json_object *o;
            int32_t n;

            for (; msg->rem > 0; msg->rem--, io_len++, (*msg->pbin_pos)++) {
                o = json_object_array_get_idx(msg->bin_obj, *msg->pbin_pos);
                /* If not enough bytes */
                if (o == NULL) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN;
                }
                /* If supposed byte is not an int */
                if (json_object_get_type(o) != json_type_int) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN;
                }
                n = json_object_get_int(o);
                /* If supposed byte value is out of range */
                if (n < 0 || n > UINT8_MAX) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN;
                }
                io_buf[io_len] = (uint8_t)n;
                /* If the I/O buffer is full */
                if (io_len >= io_size) {
                    io_full = true;
                    break;
                }
            }
        } else {
            uint8_t b;
            size_t l;

            while (msg->rem > 0) {
                b = *(uint8_t *)*msg->ptxt_ptr;
                l = tlog_json_msg_utf8_len(b);
                /* If found invalid UTF-8 character in text */
                if (l == 0) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT;
                }
                /* If character crosses text boundary */
                if (l > *msg->ptxt_len) {
                    return TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT;
                }
                /* If character crosses the I/O buffer boundary */
                if (io_len + l > io_size) {
                    io_full = true;
                    break;
                }
                while (true) {
                    io_buf[io_len] = b;
                    io_len++;
                    (*msg->ptxt_len)--;
                    (*msg->ptxt_ptr)++;
                    l--;
                    if (l == 0) {
                        break;
                    }
                    b = *(uint8_t *)*msg->ptxt_ptr;
                }
                msg->rem--;
            }
        }
    } while (!io_full);

    if (io_len > 0) {
        tlog_pkt_init_io(pkt, &msg->pos, pkt_output, io_buf, false, io_len);
    }

    return TLOG_RC_OK;
}

void
tlog_json_msg_cleanup(struct tlog_json_msg *msg)
{
    assert(tlog_json_msg_is_valid(msg));
    if (msg->obj != NULL) {
        json_object_put(msg->obj);
        msg->obj = NULL;
    }
    assert(tlog_json_msg_is_valid(msg));
}
