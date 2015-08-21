/*
 * Tlog JSON message parser.
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
#include "tlog/rc.h"
#include "tlog/misc.h"
#include "tlog/msg.h"

bool
tlog_msg_is_valid(const struct tlog_msg *msg)
{
    if (msg == NULL)
        return false;

    if (msg->obj == NULL)
        return true;

    if (!(msg->host != NULL &&
          msg->user != NULL &&
          msg->session > 0 &&
          tlog_msg_type_is_valid(msg->type)))
        return false;

    switch (msg->type) {
    case TLOG_MSG_TYPE_IO:
        if (!(msg->data.io.timing_ptr != NULL &&
              msg->data.io.in_txt_ptr != NULL &&
              msg->data.io.in_bin_obj != NULL &&
              msg->data.io.in_bin_pos >= 0 &&
              msg->data.io.out_txt_ptr != NULL &&
              msg->data.io.out_bin_obj != NULL &&
              msg->data.io.out_bin_pos >= 0))
            return false;
        break;
    default:
        break;
    }

    return true;
}

bool
tlog_msg_is_void(const struct tlog_msg *msg)
{
    assert(tlog_msg_is_valid(msg));
    return msg->obj == NULL;
}

tlog_grc
tlog_msg_init(struct tlog_msg *msg, struct json_object *obj)
{
    struct json_object *o;
    int64_t session;
    int64_t id;
    long long int tv_sec;
    long int tv_nsec;
    const char *type;

    assert(msg != NULL);

    memset(msg, 0, sizeof(*msg));

    if (obj == NULL) {
        assert(tlog_msg_is_valid(msg));
        return true;
    }

#define GET_FIELD(_name_token, _type_token) \
    do {                                                            \
        if (!json_object_object_get_ex(obj, #_name_token, &o))      \
            return TLOG_RC_MSG_FIELD_MISSING;                       \
        if (json_object_get_type(o) != json_type_##_type_token)     \
            return TLOG_RC_MSG_FIELD_INVALID_TYPE;                  \
    } while (0)

    GET_FIELD(host, string);
    msg->host = json_object_get_string(o);

    GET_FIELD(user, string);
    msg->user = json_object_get_string(o);

    GET_FIELD(session, int);
    session = json_object_get_int64(o);
    if (session < 1 || session > UINT_MAX)
        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
    msg->session = (unsigned int)session;

    GET_FIELD(id, int);
    id = json_object_get_int64(o);
    if (id < 0
#if INT64_MAX > SIZE_MAX
        || id > (int64_t)SIZE_MAX
#endif
    ) {
        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
    }
    msg->id = (size_t)id;

    GET_FIELD(pos, string);
    if (sscanf(json_object_get_string(o), "%lld.%ld",
               &tv_sec, &tv_nsec) != 2)
        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
    msg->pos.tv_sec = tv_sec;
    msg->pos.tv_sec = tv_nsec;

    GET_FIELD(type, string);
    type = json_object_get_string(o);
    if (strcmp(type, "window") == 0) {
        int width;
        int height;

        msg->type = TLOG_MSG_TYPE_WINDOW;

        GET_FIELD(width, int);
        width = json_object_get_int(o);
        if (width < 0 || width > USHRT_MAX)
            return TLOG_RC_MSG_FIELD_INVALID_VALUE;
        msg->data.window.width = (unsigned short int)width;

        GET_FIELD(height, int);
        height = json_object_get_int(o);
        if (height < 0 || height > USHRT_MAX)
            return TLOG_RC_MSG_FIELD_INVALID_VALUE;
        msg->data.window.height = (unsigned short int)height;
    } else if (strcmp(type, "io") == 0) {
        msg->type = TLOG_MSG_TYPE_IO;

        GET_FIELD(timing, string);
        msg->data.io.timing_ptr = json_object_get_string(o);

        GET_FIELD(in_txt, string);
        msg->data.io.in_txt_ptr = json_object_get_string(o);
        msg->data.io.in_txt_len = (size_t)json_object_get_string_len(o);

        GET_FIELD(in_bin, array);
        msg->data.io.in_bin_obj = o;
        msg->data.io.in_bin_pos = 0;

        GET_FIELD(out_txt, string);
        msg->data.io.out_txt_ptr = json_object_get_string(o);
        msg->data.io.out_txt_len = (size_t)json_object_get_string_len(o);

        GET_FIELD(out_bin, array);
        msg->data.io.out_bin_obj = o;
        msg->data.io.out_bin_pos = 0;
    } else {
        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
    }
#undef GET_FIELD

    msg->obj = json_object_get(obj);
    assert(tlog_msg_is_valid(msg));
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
tlog_msg_utf8_len(uint8_t b)
{
    if ((b & 0x80) == 0)
        return 1;
    else if ((b & 0xe0) == 0xc0)
        return 2;
    else if ((b & 0xf0) == 0xe0)
        return 3;
    else if ((b & 0xf8) == 0xf0)
        return 4;
    else
        return 0;
};

/**
 * Read an I/O packet from the message being parsed.
 *
 * @param msg       The message to read a packet from, must be I/O type.
 * @param pkt       The packet to read into, must be void, will be void if
 *                  there are no more packets in the message.
 * @param io_buf    Pointer to buffer for writing I/O data to, which will be
 *                  referred to from the I/O packets as "not owned".
 * @param io_size   Size of the I/O buffer io_buf.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_msg_read_io(struct tlog_msg *msg, struct tlog_pkt *pkt,
                 uint8_t *io_buf, size_t io_size)
{
    struct tlog_msg_data_io    *io;
    size_t io_len = 0;

    assert(tlog_msg_is_valid(msg));
    assert(msg->type == TLOG_MSG_TYPE_IO);
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));
    assert(io_buf != NULL);
    assert(io_size >= TLOG_MSG_IO_SIZE_MIN);

    io = &msg->data.io;

    while (true) {
        /*
         * Read next timing record if the current one is spent
         */
        if (io->rem == 0) {
            char type[2];
            int read;
            size_t first_len;
            size_t second_len;
            struct timespec delay;

            /* If reached the end of timing and so the end of data */
            if (*io->timing_ptr == 0)
                return TLOG_RC_OK;

            if (sscanf(io->timing_ptr, "%1[][><+]%zu%n",
                       type, &first_len, &read) < 2)
                return TLOG_RC_MSG_FIELD_INVALID_VALUE;
            io->timing_ptr += read;

            if (*type == '[' || *type == ']') {
                if (sscanf(io->timing_ptr, "/%zu%n", &second_len, &read) < 1)
                    return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                io->timing_ptr += read;
            } else {
                second_len = 0;
            }
            switch (*type) {
                case '+':
                    delay.tv_sec = first_len / 1000;
                    delay.tv_nsec = first_len % 1000 * 1000000;
                    tlog_timespec_add(&msg->pos, &delay, &msg->pos);
                    continue;
                case '<':
                    io->output = false;
                    io->binary = false;
                    io->rem = first_len;
                    io->ptxt_ptr = &io->in_txt_ptr;
                    io->ptxt_len = &io->in_txt_len;
                case '[':
                    io->output = false;
                    io->binary = true;
                    io->rem = second_len;
                    io->bin_obj = io->in_bin_obj;
                    io->pbin_pos = &io->in_bin_pos;
                case '>':
                    io->output = true;
                    io->binary = false;
                    io->rem = first_len;
                    io->ptxt_ptr = &io->out_txt_ptr;
                    io->ptxt_len = &io->out_txt_len;
                case ']':
                    io->output = true;
                    io->binary = true;
                    io->rem = second_len;
                    io->bin_obj = io->out_bin_obj;
                    io->pbin_pos = &io->out_bin_pos;
            }

            if (io->binary) {
                size_t l;
                /* Skip replacement characters */
                for (; first_len > 0; first_len--) {
                    /* If not enough text */
                    if (*io->ptxt_len == 0)
                        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                    l = tlog_msg_utf8_len(*(uint8_t *)*io->ptxt_ptr);
                    /* If found invalid UTF-8 character in text */
                    if (l == 0)
                        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                    /* If character crosses text boundary */
                    if (l > *io->ptxt_len)
                        return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                    *io->ptxt_len -= l;
                    *io->ptxt_ptr += l;
                }
            }
        }

        /*
         * Return (a piece of) I/O
         */
        if (io->binary) {
            struct json_object *o;
            int32_t n;

            for (; io->rem > 0 && io_len < io_size;
                 io->rem--, io_len++, *io->pbin_pos++) {
                o = json_object_array_get_idx(io->bin_obj, *io->pbin_pos);
                /* If not enough bytes */
                if (o == NULL)
                    return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                /* If supposed byte is not an int */
                if (json_object_get_type(o) != json_type_int)
                    return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                n = json_object_get_int(o);
                /* If supposed byte value is out of range */
                if (n < 0 || n > UINT8_MAX)
                    return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                io_buf[io_len] = (uint8_t)n;
            }
        } else {
            uint8_t b;
            size_t l;

            while (io->rem > 0) {
                b = *(uint8_t *)*io->ptxt_ptr;
                l = tlog_msg_utf8_len(b);
                /* If found invalid UTF-8 character in text */
                if (l == 0)
                    return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                /* If character crosses text boundary */
                if (l > *io->ptxt_len)
                    return TLOG_RC_MSG_FIELD_INVALID_VALUE;
                if (io_len + l > io_size)
                    break;
                while (true) {
                    io_buf[io_len] = b;
                    io_len++;
                    (*io->ptxt_len)--;
                    (*io->ptxt_ptr)++;
                    l--;
                    if (l == 0)
                        break;
                    b = *(uint8_t *)*io->ptxt_ptr;
                }
                io->rem--;
            }
        }
        break;

        /* TODO: concatenate similar timing records into a single packet */
    }

    tlog_pkt_init_io(pkt, &msg->pos, io->output,
                     io_buf, false, io_len);
    return TLOG_RC_OK;
}

tlog_grc
tlog_msg_read(struct tlog_msg *msg, struct tlog_pkt *pkt,
              uint8_t *io_buf, size_t io_size)
{
    assert(tlog_msg_is_valid(msg));
    assert(!tlog_msg_is_void(msg));
    assert(tlog_pkt_is_valid(pkt));
    assert(tlog_pkt_is_void(pkt));
    assert(io_buf != NULL);
    assert(io_size >= TLOG_MSG_IO_SIZE_MIN);

    switch (msg->type) {
    case TLOG_MSG_TYPE_WINDOW:
        if (!msg->data.window.read) {
            tlog_pkt_init_window(pkt, &msg->pos,
                                 msg->data.window.width,
                                 msg->data.window.height);
            msg->data.window.read = true;
        }
        return TLOG_RC_OK;
    case TLOG_MSG_TYPE_IO:
        return tlog_msg_read_io(msg, pkt, io_buf, io_size);
    default:
        assert(false);
        return TLOG_RC_OK;
    }
}

void
tlog_msg_cleanup(struct tlog_msg *msg)
{
    assert(tlog_msg_is_valid(msg));
    if (msg->obj != NULL) {
        json_object_put(msg->obj);
        msg->obj = NULL;
    }
    assert(tlog_msg_is_valid(msg));
}
