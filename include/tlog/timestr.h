/**
 * @file
 * @brief Timestamp string
 *
 * Timestamp string has format HH:MM:SS.sss, where any part except SS can
 * omitted.
 */
/*
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

#ifndef _TLOG_TIMESTR_H
#define _TLOG_TIMESTR_H

#include <stdlib.h>
#include <stdbool.h>

/** The timestamp string parser state */
struct tlog_timestr_parser {
    /** List of timestamp components being received */
    long        comp_list[3];
    /** Number of timestamp components received so far */
    size_t      comp_num;
    /** True if received a decimal point */
    bool        got_point;
    /** Length of the fraction part */
    size_t      frac_len;
    /** Value of the fraction (multiplied by 10^frac_len) */
    long        frac_val;
};

/** Timestamp string parser initializer */
#define TLOG_TIMESTR_PARSER_INIT \
    (struct tlog_timestr_parser){.comp_num = 0, .got_point = false}

/**
 * Reset a timestamp string parser.
 * Must be called before parsing a new string.
 *
 * @param parser    The timestamp string parser to reset.
 */
extern void tlog_timestr_parser_reset(struct tlog_timestr_parser *parser);

/**
 * Check if a timestamp string parser is valid.
 *
 * @param parser    The parser to check.
 *
 * @return True if the parser is valid, false otherwise.
 */
extern bool tlog_timestr_parser_is_valid(
                                const struct tlog_timestr_parser *parser);

/**
 * Feed a character into a timestamp string parser.
 *
 * @param parser    The timestamp string parser to feed the character into.
 * @param c         The character to feed into the parser.
 *
 * @return True if the character was accepted, false otherwise.
 */
extern bool tlog_timestr_parser_feed(struct tlog_timestr_parser *parser,
                                     char c);

/**
 * Yield a timespec value from a timestamp string parser state.
 *
 * @param parser    The timestamp string parser to yield the timespec from.
 * @param pts       Location for the yielded timespec value.
 *
 * @return True if the parser state could be represented as a timespec and pts
 *         location was updated. False if an overflow occurred, and pts
 *         location wasn't updated.
 */
extern bool tlog_timestr_parser_yield(const struct tlog_timestr_parser *parser,
                                      struct timespec *pts);

/**
 * Convert a timestamp string to a timespec, ignoring any leading or trailing
 * whitespace.
 *
 * @param timestr   The timestamp string to convert.
 * @param pts       Location for the resulting timespec value.
 *
 * @return True if converted succesfully and pts location was updated,
 *         false if the timestamp string was invalid and pts location wasn't
 *         updated.
 */
extern bool tlog_timestr_to_timespec(const char *timestr,
                                     struct timespec *pts);

#endif /* _TLOG_TIMESTR_H */
