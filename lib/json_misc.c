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
#include <tlog/misc.h>
#include <string.h>
#include <assert.h>

tlog_grc
tlog_json_overlay(struct json_object    **presult,
                  struct json_object     *lower,
                  struct json_object     *upper)
{
    tlog_grc grc;
    struct json_object *result;
    struct json_object *result_item;
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
        struct json_object *result_item;

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
