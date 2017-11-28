/*
 * Miscellaneous JSON handling functions.
 *
 * Copyright (C) 2016 Red Hat
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

#include <config.h>
#include <tlog/json_misc.h>
#include <tlog/rc.h>
#include <tlog/utf8.h>
#include <tlog/misc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

tlog_grc
tlog_json_overlay(struct json_object    **presult,
                  struct json_object     *lower,
                  struct json_object     *upper)
{
    tlog_grc grc;
    struct json_object *result;
    struct json_object *result_item = NULL;
    enum json_type lower_type;
    enum json_type upper_type;

    assert(presult != NULL);

    lower_type = json_object_get_type(lower);
    upper_type = json_object_get_type(upper);

    /* If it's an array (we need to duplicate) */
    if (upper_type == json_type_array) {
        int length = json_object_array_length(upper);
        int i;
        struct json_object *upper_item;

        result = json_object_new_array();
        if (result == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto failure;
        }

        for (i = 0; i < length; i++) {
            upper_item = json_object_array_get_idx(upper, i);
            result_item = NULL;
            grc = tlog_json_overlay(&result_item, NULL, upper_item);
            if (grc != TLOG_RC_OK) {
                goto failure;
            }
            if (json_object_array_put_idx(result, i, result_item) < 0) {
                grc = TLOG_GRC_ERRNO;
                goto failure;
            }
            result_item = NULL;
        }
    /* Else if it's an object (we need to merge or duplicate) */
    } else if (upper_type == json_type_object) {
        struct json_object *lower_item;

        result = json_object_new_object();
        if (result == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto failure;
        }

        /* If lower is also an object */
        if (lower_type == json_type_object) {
            /* Copy lower object items that are not in the upper object */
            json_object_object_foreach(lower, lower_key, lower_item) {
                if (json_object_object_get_ex(upper, lower_key, NULL)) {
                    continue;
                }
                result_item = NULL;
                grc = tlog_json_overlay(&result_item, NULL, lower_item);
                if (grc != TLOG_RC_OK) {
                    goto failure;
                }
                /* TODO Handle failure with newer JSON-C */
                json_object_object_add(result, lower_key, result_item);
                result_item = NULL;
            }
        } else {
            lower_item = NULL;
        }

        /* Merge upper object items on top of lower object items */
        json_object_object_foreach(upper, upper_key, upper_item) {
            if (lower_type == json_type_object) {
                json_object_object_get_ex(lower, upper_key, &lower_item);
            }
            result_item = NULL;
            grc = tlog_json_overlay(&result_item, lower_item, upper_item);
            if (grc != TLOG_RC_OK) {
                goto failure;
            }
            /* TODO Handle failure with newer JSON-C */
            json_object_object_add(result, upper_key, result_item);
            result_item = NULL;
        }
    /* Else if it's NULL (we use lower) */
    } else if (upper_type == json_type_null) {
        /* Duplicate lower */
        result = NULL;
        grc = tlog_json_overlay(&result, NULL, lower);
        if (grc != TLOG_RC_OK) {
            goto failure;
        }
    /* Else it's an immutable type (we can reference) */
    } else {
        result = json_object_get(upper);
    }

    json_object_put(*presult);
    *presult = result;
    return TLOG_RC_OK;

failure:
    json_object_put(result_item);
    json_object_put(result);
    return grc;
}

tlog_grc
tlog_json_object_object_add_path(struct json_object* obj,
                                 const char *path,
                                 struct json_object *val)
{
    tlog_grc grc;
    char *buf = NULL;
    char *name_start;
    char *name_end;
    struct json_object *sub_obj = NULL;

    assert(obj != NULL);
    assert(path != NULL);

    buf = strdup(path);
    if (buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto cleanup;
    }

    for (name_start = buf;
         *(name_end = strchrnul(name_start, '.')) != '\0';
         name_start = name_end + 1) {
        *name_end = '\0';
        if (!json_object_object_get_ex(obj, name_start, &sub_obj)) {
            sub_obj = json_object_new_object();
            if (sub_obj == NULL) {
                grc = TLOG_GRC_ERRNO;
                goto cleanup;
            }
            /* TODO Handle failure with newer JSON-C */
            json_object_object_add(obj, name_start, sub_obj);
        }
        obj = sub_obj;
        sub_obj = NULL;
    }

    /* TODO Handle failure with newer JSON-C */
    json_object_object_add(obj, name_start, val);
    grc = TLOG_RC_OK;

cleanup:
    json_object_put(sub_obj);
    free(buf);
    return grc;
}

tlog_grc
tlog_json_object_object_get_path(struct json_object *obj,
                                 const char *path,
                                 struct json_object **pval,
                                 bool *pfound)
{
    tlog_grc grc;
    struct json_object *val = NULL;
    bool found = false;
    char *buf = NULL;
    char *name_start;
    char *name_end;
    struct json_object *sub_obj;

    assert(path != NULL);

    buf = strdup(path);
    if (buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto cleanup;
    }

    for (name_start = buf;
         *(name_end = strchrnul(name_start, '.')) != '\0';
         name_start = name_end + 1) {
        *name_end = '\0';
        if (json_object_get_type(obj) == json_type_object &&
            json_object_object_get_ex(obj, name_start, &sub_obj)) {
            obj = sub_obj;
        } else {
            goto exit;
        }
    }

    if (json_object_get_type(obj) == json_type_object) {
        found = json_object_object_get_ex(obj, name_start, &val);
    }

exit:
    if (pval != NULL) {
        *pval = val;
    }
    if (pfound != NULL) {
        *pfound = found;
    }

    grc = TLOG_RC_OK;
cleanup:
    free(buf);
    return grc;
}

tlog_grc
tlog_json_object_from_file(struct json_object **pconf, const char *path)
{
    tlog_grc grc;
    int fd = -1;
    size_t size = 0;
    size_t len = 0;
    char *buf = NULL;
    char *new_buf;
    size_t new_size;
    ssize_t rc;
    struct json_object *conf;
    enum json_tokener_error jerr;

    assert(pconf != NULL);
    assert(path != NULL);

    /* Open the file */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        grc = TLOG_GRC_ERRNO;
        goto cleanup;
    }

    /* Allocate the buffer */
    size = 2048;
    buf = malloc(size);
    if (buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto cleanup;
    }

    /* Read the whole file */
    while (true) {
        rc = read(fd, buf + len, size - len - 1);
        if (rc < 0) {
            grc = TLOG_GRC_ERRNO;
            goto cleanup;
        } else if (rc == 0) {
            break;
        }

        len += rc;

        if (len > size / 2) {
            new_size = size * 2;
            new_buf = realloc(buf, new_size);
            if (new_buf == NULL) {
                grc = TLOG_GRC_ERRNO;
                goto cleanup;
            }
            buf = new_buf;
            size = new_size;
        }
    }
    buf[len] = '\0';

    /* Parse the contents */
    conf = json_tokener_parse_verbose(buf, &jerr);
    if (conf == NULL) {
        grc = TLOG_GRC_FROM(json, jerr);
        goto cleanup;
    }

    *pconf = conf;
    grc = TLOG_RC_OK;
cleanup:
    free(buf);
    if (fd >= 0) {
        close(fd);
    }
    return grc;
}

/**
 * Request space for the next escaped character.
 *
 * @param pres_len      Location of/for the total required space to add the
 *                      character length to.
 * @param pout_of_space Location of/for a flag signifying that the output
 *                      space ended before or now.
 * @param pout_len      Location of/for the remaining output space. Only
 *                      modified if there was enough space.
 * @param req           The requested space in bytes.
 *
 * @return True if the escaped character can be output, false otherwise.
 */
static inline bool
tlog_json_esc_buf_req(size_t *pres_len, bool *pout_of_space,
                      size_t *pout_len, size_t req)
{
    *pres_len += req;
    if (*pout_of_space) {
        return false;
    }
    if (*pout_len <= req) {
        *pout_of_space = true;
        return false;
    }
    *pout_len -= req;
    return true;
}

size_t
tlog_json_esc_buf(char *out_ptr, size_t out_len,
                  const char *in_ptr, size_t in_len)
{
    bool out_of_space = false;
    size_t res_len = 0;
    struct tlog_utf8 utf8 = TLOG_UTF8_INIT;

    assert(out_ptr != NULL || out_len == 0);
    assert(in_ptr != NULL || in_len == 0);
    assert(tlog_utf8_buf_is_valid(in_ptr, in_len));

#define REQ(_n) \
    tlog_json_esc_buf_req(&res_len, &out_of_space, &out_len, (_n))

    while (in_len > 0) {
        if (tlog_utf8_add(&utf8, *(const uint8_t *)in_ptr)) {
            in_len--;
            in_ptr++;
        }
        if (!tlog_utf8_is_ended(&utf8)) {
            continue;
        }
        assert(utf8.len != 0);

        if (!tlog_utf8_is_complete(&utf8) || utf8.len != 1) {
            if (REQ(utf8.len)) {
                memcpy(out_ptr, utf8.buf, utf8.len);
                out_ptr += utf8.len;
            }
        } else {
            uint8_t c = *utf8.buf;
            switch (c) {
            case '"':
            case '\\':
                if (REQ(2)) {
                    *out_ptr++ = '\\';
                    *out_ptr++ = c;
                }
                break;
#define ESC_CASE(_c, _e) \
            case _c:                    \
                if (REQ(2)) {             \
                    *out_ptr++ = '\\';  \
                    *out_ptr++ = _e;    \
                }                       \
                break;
            ESC_CASE('\b', 'b');
            ESC_CASE('\f', 'f');
            ESC_CASE('\n', 'n');
            ESC_CASE('\r', 'r');
            ESC_CASE('\t', 't');
#undef ESC
            default:
                if (c < 0x20 || c == 0x7f) {
                    if (REQ(6)) {
                        *out_ptr++ = '\\';
                        *out_ptr++ = 'u';
                        *out_ptr++ = '0';
                        *out_ptr++ = '0';
                        *out_ptr++ = tlog_nibble_digit(c >> 4);
                        *out_ptr++ = tlog_nibble_digit(c & 0xf);
                    }
                } else {
                    if (REQ(1)) {
                        *out_ptr++ = c;
                    }
                }
                break;
            }
        }
        tlog_utf8_reset(&utf8);
    }

    if (tlog_utf8_is_started(&utf8) && !tlog_utf8_is_ended(&utf8)) {
        if (REQ(utf8.len)) {
            memcpy(out_ptr, utf8.buf, utf8.len);
            out_ptr += utf8.len;
        }
    }

    if (out_len > 0) {
        *out_ptr = 0;
    }
    res_len++;

#undef REQ

    return res_len;
}

char *
tlog_json_aesc_buf(const char *in_ptr, size_t in_len)
{
    char *out_ptr;
    size_t out_len_calc;
    size_t out_len_out;

    assert(in_ptr != NULL || in_len == 0);
    assert(tlog_utf8_buf_is_valid(in_ptr, in_len));

    out_len_calc = tlog_json_esc_buf(NULL, 0, in_ptr, in_len);
    out_ptr = malloc(out_len_calc);
    if (out_ptr != NULL) {
        out_len_out = tlog_json_esc_buf(out_ptr, out_len_calc,
                                        in_ptr, in_len);
        assert(out_len_out == out_len_calc);
#if NDEBUG
        (void)out_len_out;
#endif
    }
    return out_ptr;
}
