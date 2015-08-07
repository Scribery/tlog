/*
 * Tlog JSON message parsing state.
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

bool
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
        if (!json_object_object_get_ex(obj, #_name_token, &o) ||    \
            json_object_get_type(o) != json_type_##_type_token)     \
            return false;                                           \
    } while (0)

    GET_FIELD(host, string);
    msg->host = json_object_get_string(o);

    GET_FIELD(user, string);
    msg->user = json_object_get_string(o);

    GET_FIELD(session, int);
    session = json_object_get_int64(o);
    if (session < 1 || session > UINT_MAX)
        return false;
    msg->session = (unsigned int)session;

    GET_FIELD(id, int);
    id = json_object_get_int64(o);
    if (id < 0
#if INT64_MAX > SIZE_MAX
        || id > (int64_t)SIZE_MAX
#endif
    ) {
        return false;
    }
    msg->id = (size_t)id;

    GET_FIELD(pos, string);
    if (sscanf(json_object_get_string(o), "%lld.%ld",
               &tv_sec, &tv_nsec) != 2)
        return false;
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
            return false;
        msg->data.window.width = (unsigned short int)width;

        GET_FIELD(height, int);
        height = json_object_get_int(o);
        if (height < 0 || height > USHRT_MAX)
            return false;
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
        return false;
    }
#undef GET_FIELD

    msg->obj = obj;
    assert(tlog_msg_is_valid(msg));
    return true;
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
