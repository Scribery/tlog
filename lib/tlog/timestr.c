/*
 * Timestamp string
 *
 * Copyright (C) 2017 Red Hat
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

#include <tlog/timestr.h>
#include <tlog/timespec.h>
#include <tlog/misc.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

void
tlog_timestr_parser_reset(struct tlog_timestr_parser *parser)
{
    assert(parser != NULL);
    *parser = TLOG_TIMESTR_PARSER_INIT;
}

bool
tlog_timestr_parser_is_valid(const struct tlog_timestr_parser *parser)
{
    return parser != NULL &&
           (parser->comp_num <= TLOG_ARRAY_SIZE(parser->comp_list)) &&
           (!parser->got_point ||
            (parser->frac_len <= 9 && parser->frac_val <= TLOG_TIMESPEC_NSEC_PER_SEC - 1));
}

bool
tlog_timestr_parser_feed(struct tlog_timestr_parser *parser, char c)
{
    long digit;
    long comp;

    assert(tlog_timestr_parser_is_valid(parser));

    switch (c) {
        case '0' ... '9':
            digit = c - '0';
            if (parser->got_point) {
                if (parser->frac_len >= 9) {
                    return false;
                }
                parser->frac_len++;
                parser->frac_val = parser->frac_val * 10 + digit;
            } else {
                if (parser->comp_num == 0) {
                    parser->comp_num++;
                    comp = 0;
                } else {
                    comp = parser->comp_list[parser->comp_num - 1];
                    if (comp > (LONG_MAX - digit) / 10) {
                        return false;
                    }
                }
                parser->comp_list[parser->comp_num - 1] = comp * 10 + digit;
            }
            return true;
        case ':':
            if (parser->got_point ||
                parser->comp_num >= TLOG_ARRAY_SIZE(parser->comp_list)) {
                return false;
            }
            parser->comp_list[parser->comp_num] = 0;
            parser->comp_num++;
            return true;
        case '.':
            if (parser->got_point) {
                return false;
            }
            parser->got_point = true;
            parser->frac_len = 0;
            parser->frac_val = 0;
            return true;
        default:
            return false;
    }
}

bool
tlog_timestr_parser_yield(const struct tlog_timestr_parser *parser,
                          struct timespec *pts)
{
    static const long mul_list[TLOG_ARRAY_SIZE(parser->comp_list)] =
                                                        {1, 60, 60 * 60};
    struct timespec ts = TLOG_TIMESPEC_ZERO;
    size_t i;
    long mul;
    long comp;

    assert(tlog_timestr_parser_is_valid(parser));
    assert(pts != NULL);

    /* Accumulate components */
    for (i = 0; i < parser->comp_num; i++) {
        mul = mul_list[parser->comp_num - 1 - i];
        comp = parser->comp_list[i];
        if (comp > LONG_MAX / mul) {
            return false;
        }
        comp *= mul;
        if (comp > LONG_MAX - ts.tv_sec) {
            return false;
        }
        ts.tv_sec += comp;
    }
    /* Add fraction, shifted if necessary */
    if (parser->got_point) {
        ts.tv_nsec = parser->frac_val;
        for (i = parser->frac_len; i < 9; i++) {
            ts.tv_nsec *= 10;
        }
    }

    *pts = ts;
    return true;
}

bool
tlog_timestr_to_timespec(const char *timestr, struct timespec *pts)
{
    struct tlog_timestr_parser parser = TLOG_TIMESTR_PARSER_INIT;
    const char *p;

    /* Skip leading whitespace */
    for (p = timestr; isspace(*p); p++);

    /* Parse the timestamp string */
    for (; *p != 0; p++) {
        if (!tlog_timestr_parser_feed(&parser, *p)) {
            break;
        }
    }

    /* Skip trailing whitespace */
    for (; isspace(*p); p++);

    /* Check there's nothing else and yield the parsing result */
    return (*p == '\0') && tlog_timestr_parser_yield(&parser, pts);
}
