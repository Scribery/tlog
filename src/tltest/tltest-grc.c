/*
 * Tlog grc (global return code) module test.
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <json_tokener.h>
#include <netdb.h>
#include <curl/curl.h>
#include <tlog/grc.h>
#include <tlog/rc.h>

bool
test(const char *file, int line, const char *name, bool res)
{
    fprintf(stderr, "%s %s:%d %s\n", (res ? "PASS" : "FAIL"),
            file, line, name);
    return res;
}

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _expr) \
    passed = test(__FILE__, __LINE__, #_name_token, _expr) && passed

    TEST(invalid, !tlog_grc_is_valid(INT_MAX));

#define native_from(x)  (x)
#define errno_from(x)   (-(x) - 10000)
#define gai_from(x)    (-(x) + 10000)
#define json_from(x)    ((x) + 20000)
#define curl_from(x)    ((x) + 30000)

#define TEST_CODE(_range_name_token, _rc_name_token, _value, _strerror) \
    do {                                                                    \
        int rc = _value;                                                    \
        tlog_grc grc = tlog_grc_from(&tlog_grc_range_##_range_name_token,   \
                                     rc);                                   \
                                                                            \
        TEST(_range_name_token##_##_rc_name_token##_from,                   \
             grc == _range_name_token##_from(rc));                          \
        TEST(_range_name_token##_##_rc_name_token##_is_valid,               \
             tlog_grc_is_valid(grc));                                       \
        TEST(_range_name_token##_##_rc_name_token##_is,                     \
             tlog_grc_is(&tlog_grc_range_##_range_name_token, grc));        \
        TEST(_range_name_token##_##_rc_name_token##_strerror_match,         \
             tlog_grc_strerror(grc) == _strerror(rc));                      \
        TEST(_range_name_token##_##_rc_name_token##_to,                     \
             tlog_grc_to(&tlog_grc_range_##_range_name_token, grc) == rc);  \
    } while (0)

    TEST_CODE(native, ok, TLOG_RC_OK, tlog_rc_strerror);
    TEST_CODE(native, failure, TLOG_RC_FAILURE, tlog_rc_strerror);
    TEST_CODE(errno, einval, EINVAL, strerror);
    TEST_CODE(errno, efault, EFAULT, strerror);
    TEST_CODE(gai, again, EAI_AGAIN, gai_strerror);
    TEST_CODE(gai, noname, EAI_NONAME, gai_strerror);
    TEST_CODE(json, success, json_tokener_success, json_tokener_error_desc);
    TEST_CODE(json, error_size, json_tokener_error_depth, json_tokener_error_desc);
    TEST_CODE(curl, failed_init, CURLE_FAILED_INIT, curl_easy_strerror);
    TEST_CODE(curl, couldnt_resolve_host, CURLE_COULDNT_RESOLVE_HOST,
              curl_easy_strerror);

    return !passed;
}
