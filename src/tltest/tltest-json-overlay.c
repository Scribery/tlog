/*
 * tlog_json_overlay function test.
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

#include <tlog/json_misc.h>
#include <tltest/misc.h>
#include <tlog/grc.h>
#include <tlog/rc.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

bool
test(const char *file,
     int line,
     const char *name,
     const char *lower_text,
     const char *upper_text,
     const char *exp_result_text)
{
    bool passed = true;
    tlog_grc grc;
    struct json_object *lower = NULL;
    struct json_object *upper = NULL;
    struct json_object *result = NULL;
    size_t exp_result_len = strlen(exp_result_text);
    const char *res_result_text;
    size_t res_result_len;

    lower = json_tokener_parse(lower_text);
    assert(lower != NULL);
    upper = json_tokener_parse(upper_text);
    assert(upper != NULL);

    grc = tlog_json_overlay(&result, lower, upper);
    if (grc != TLOG_RC_OK) {
        passed = false;
        fprintf(stderr, "tlog_json_overlay failed: %s",
                tlog_grc_strerror(grc));
    }

    json_object_put(lower);
    lower = NULL;
    json_object_put(upper);
    upper = NULL;

    res_result_text = json_object_to_json_string(result);
    if (res_result_text == NULL) {
        passed = false;
        fprintf(stderr, "json_object_to_json_string failed: %s",
                strerror(errno));
        res_result_len = 0;
    } else {
        res_result_len = strlen(res_result_text);
    }

    if (res_result_len != exp_result_len ||
        memcmp(res_result_text, exp_result_text, res_result_len) != 0) {
        passed = false;
        fprintf(stderr, "%s: result mismatch:\n", name);
        tlog_test_diff(stderr,
                       (const uint8_t *)res_result_text,
                       res_result_len,
                       (const uint8_t *)exp_result_text,
                       exp_result_len);
    }

    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, name);

    json_object_put(lower);
    json_object_put(upper);
    json_object_put(result);

    return passed;
}

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _lower, _upper, _result) \
    passed = test(__FILE__, __LINE__, #_name_token,   \
                  _lower, _upper, _result) && passed  \

    TEST(empty, "{ }", "{ }", "{ }");

    TEST(atomic_lower,
         "{ \"x\": 1 }",
         "{ }",
         "{ \"x\": 1 }");

    TEST(atomic_upper,
         "{ }",
         "{ \"x\": 1 }",
         "{ \"x\": 1 }");

    TEST(same_atomic_both,
         "{ \"x\": 1 }",
         "{ \"x\": 2 }",
         "{ \"x\": 2 }");

    TEST(different_atomic_both,
         "{ \"x\": 1 }",
         "{ \"y\": 2 }",
         "{ \"x\": 1, \"y\": 2 }");

    TEST(two_same_atomics_both,
         "{ \"x\": 1, \"y\": 2 }",
         "{ \"x\": 3, \"y\": 4 }",
         "{ \"x\": 3, \"y\": 4 }");

    TEST(object_lower,
         "{ \"z\": { \"x\": 1 } }",
         "{ }",
         "{ \"z\": { \"x\": 1 } }");

    TEST(object_upper,
         "{ }",
         "{ \"z\": { \"x\": 1 } }",
         "{ \"z\": { \"x\": 1 } }");

    TEST(array_lower,
         "{ \"z\": [ 1, 2, 3 ] }",
         "{ }",
         "{ \"z\": [ 1, 2, 3 ] }");

    TEST(array_upper,
         "{ }",
         "{ \"z\": [ 1, 2, 3 ] }",
         "{ \"z\": [ 1, 2, 3 ] }");

    TEST(array_same_length,
         "{ \"z\": [ 1, 2, 3 ] }",
         "{ \"z\": [ 4, 5, 6 ] }",
         "{ \"z\": [ 4, 5, 6 ] }");

    TEST(array_empty_lower,
         "{ \"z\": [ ] }",
         "{ \"z\": [ 4, 5, 6 ] }",
         "{ \"z\": [ 4, 5, 6 ] }");

    TEST(array_empty_upper,
         "{ \"z\": [ 1, 2, 3 ] }",
         "{ \"z\": [ ] }",
         "{ \"z\": [ ] }");

    TEST(array_longer_upper,
         "{ \"z\": [ 1, 2 ] }",
         "{ \"z\": [ 4, 5, 6 ] }",
         "{ \"z\": [ 4, 5, 6 ] }");

    TEST(array_longer_lower,
         "{ \"z\": [ 1, 2, 3 ] }",
         "{ \"z\": [ 4, 5 ] }",
         "{ \"z\": [ 4, 5 ] }");

    TEST(object_in_array_lower,
         "{ \"z\": [ { \"x\": 1 } ] }",
         "{ \"z\": [ ] }",
         "{ \"z\": [ ] }");

    TEST(object_in_array_upper,
         "{ \"z\": [ ] }",
         "{ \"z\": [ { \"x\": 1 } ] }",
         "{ \"z\": [ { \"x\": 1 } ] }");

    TEST(object_in_array_both,
         "{ \"z\": [ { \"x\": 1 } ] }",
         "{ \"z\": [ { \"x\": 2 } ] }",
         "{ \"z\": [ { \"x\": 2 } ] }");

    TEST(nested_atomic_lower,
         "{ \"z\": { \"x\": 1 } }",
         "{ \"z\": { } }",
         "{ \"z\": { \"x\": 1 } }");

    TEST(nested_atomic_upper,
         "{ \"z\": { } }",
         "{ \"z\": { \"x\": 1 } }",
         "{ \"z\": { \"x\": 1 } }");

    TEST(nested_same_atomic_both,
         "{ \"z\": { \"x\": 1 } }",
         "{ \"z\": { \"x\": 2 } }",
         "{ \"z\": { \"x\": 2 } }");

    return !passed;
}
