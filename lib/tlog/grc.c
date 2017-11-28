/*
 * Global (function) return codes.
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

#include <limits.h>
#include <string.h>
#include <assert.h>
#include <json_tokener.h>
#include <netdb.h>
#include <curl/curl.h>
#include <tlog/misc.h>
#include <tlog/rc.h>
#include <tlog/grc.h>

static const char *
tlog_grc_range_native_strerror(int rc)
{
    return tlog_rc_strerror(rc);
}

const struct tlog_grc_range tlog_grc_range_native = {
    .min        = TLOG_RC_MIN,
    .max        = TLOG_RC_MAX_PLUS_ONE - 1,
    .add        = 0,
    .mul        = 1,
    .strerror   = tlog_grc_range_native_strerror
};

static const char *
tlog_grc_range_errno_strerror(int rc)
{
    return strerror(rc);
}

const struct tlog_grc_range tlog_grc_range_errno = {
    .min        = INT_MIN,
    .max        = -10000,
    .add        = 10000,
    .mul        = -1,
    .strerror   = tlog_grc_range_errno_strerror
};

static const char *
tlog_grc_range_systemd_strerror(int rc)
{
    return strerror(-rc);
}

const struct tlog_grc_range tlog_grc_range_systemd = {
    .min        = INT_MIN,
    .max        = -10000,
    .add        = -10000,
    .mul        = 1,
    .strerror   = tlog_grc_range_systemd_strerror
};

static const char *
tlog_grc_range_gai_strerror(int rc)
{
    return gai_strerror(rc);
}

const struct tlog_grc_range tlog_grc_range_gai = {
    .min        = 10000,
    .max        = 19999,
    .add        = -10000,
    .mul        = -1,
    .strerror   = tlog_grc_range_gai_strerror
};

static const char *
tlog_grc_range_json_strerror(int rc)
{
    return json_tokener_error_desc(rc);
}

const struct tlog_grc_range tlog_grc_range_json = {
    .min        = 20000,
    .max        = 29999,
    .add        = 20000,
    .mul        = 1,
    .strerror   = tlog_grc_range_json_strerror
};

static const char *
tlog_grc_range_curl_strerror(int rc)
{
    return curl_easy_strerror(rc);
}

const struct tlog_grc_range tlog_grc_range_curl = {
    .min        = 30000,
    .max        = 39999,
    .add        = 30000,
    .mul        = 1,
    .strerror   = tlog_grc_range_curl_strerror
};

const struct tlog_grc_range *tlog_grc_range_list[] = {
    &tlog_grc_range_native,
    &tlog_grc_range_errno,
    &tlog_grc_range_gai,
    &tlog_grc_range_json,
    &tlog_grc_range_curl
};

bool
tlog_grc_range_is_valid(const struct tlog_grc_range *range)
{
    return range != NULL &&
           range->min <= range->max &&
           range->mul != 0 &&
           range->strerror != NULL;
}

bool
tlog_grc_is(const struct tlog_grc_range *range, tlog_grc grc)
{
    assert(tlog_grc_range_is_valid(range));
    return grc >= range->min && grc <= range->max;
}

tlog_grc
tlog_grc_from(const struct tlog_grc_range *range, int rc)
{
    tlog_grc grc;
    assert(tlog_grc_range_is_valid(range));
    grc = (rc + range->add) * range->mul;
    assert(tlog_grc_is(range, grc));
    return grc;
}

int
tlog_grc_to(const struct tlog_grc_range *range, tlog_grc grc)
{
    assert(tlog_grc_range_is_valid(range));
    assert(tlog_grc_is(range, grc));
    return grc / range->mul - range->add;
}

bool
tlog_grc_is_valid(tlog_grc grc)
{
    size_t i;
    const struct tlog_grc_range *range;

    for (i = 0; i < TLOG_ARRAY_SIZE(tlog_grc_range_list); i++) {
        range = tlog_grc_range_list[i];
        if (tlog_grc_is(range, grc)) {
            return true;
        }
    }
    return false;
}

const char *
tlog_grc_strerror(tlog_grc grc)
{
    size_t i;
    const struct tlog_grc_range *range;

    assert(tlog_grc_is_valid(grc));

    for (i = 0; i < TLOG_ARRAY_SIZE(tlog_grc_range_list); i++) {
        range = tlog_grc_range_list[i];
        if (tlog_grc_is(range, grc)) {
            return range->strerror(tlog_grc_to(range, grc));
        }
    }
    assert(false);
    return "Unknown return code range";
}
