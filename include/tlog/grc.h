/**
 * @file
 * @brief Global function return codes (grc's).
 *
 * This module implements encoding a variety of return and error code types
 * into a single integer space by shifting and possibly negating them. This is
 * needed to propagate the error codes in a uniform way up an arbitrary stack
 * depth until it could be handled or the corresponding error message could be
 * printed.
 */
/*
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

#ifndef _TLOG_GRC_H
#define _TLOG_GRC_H

#include <stdbool.h>
#include <errno.h>

/** Global return code */
typedef int tlog_grc;

/**
 * Prototype for a function retrieving description for a range rc.
 *
 * @param rc    The range rc to retrieve description for.
 *
 * @return The description as a static constant string.
 */
typedef const char * (*tlog_grc_range_strerror_fn)(int rc);

/** Global return code range */
struct tlog_grc_range {
    int min;    /**< Minimum global return code value */
    int max;    /**< Maximum global return code value */
    int add;    /**< The number to add to the range rc (before multiplying)
                     when converting to grc, or to subtract from the grc
                     (after dividing) when converting to range rc */
    int mul;    /**< The number to multiply the range rc by (after adding) when
                     converting to grc, or to divide the grc by (before
                     subtracting) when converting to range rc; cannot be 0 */
    tlog_grc_range_strerror_fn strerror;    /**< Description function */
};

/**
 * Check if a grc range is valid.
 *
 * @param range     The range to check.
 *
 * @return True if the range is valid, false otherwise.
 */
extern bool tlog_grc_range_is_valid(const struct tlog_grc_range *range);

/** Tlog-native return code range (no conversion) */
extern const struct tlog_grc_range tlog_grc_range_native;
/** Errno value range (negated and shifted) */
extern const struct tlog_grc_range tlog_grc_range_errno;
/** Systemd return value range (shifted to match errno) */
extern const struct tlog_grc_range tlog_grc_range_systemd;
/** getaddrinfo return code value range (negated and shifted) */
extern const struct tlog_grc_range tlog_grc_range_gai;
/** JSON return code value range (shifted) */
extern const struct tlog_grc_range tlog_grc_range_json;
/** libcurl return code value range (shifted) */
extern const struct tlog_grc_range tlog_grc_range_curl;

/**
 * Check if a grc belongs to a range.
 *
 * @param range The range to check against.
 * @param grc   The grc to check.
 *
 * @return True if the grc belongs to the range.
 */
extern bool tlog_grc_is(const struct tlog_grc_range *range, tlog_grc grc);

/**
 * Check if a grc belongs to a range.
 *
 * @param \_range_name_token    Range variable name without tlog_grc_ prefix.
 * @param \_grc                 The grc to check.
 *
 * @return True if the grc belongs to the range.
 */
#define TLOG_GRC_IS(_range_name_token, _grc) \
    tlog_grc_is(&tlog_grc_range_##_range_name_token, _grc)

/**
 * Convert a range rc to a grc.
 *
 * @param range The range to convert from.
 * @param rc    The range rc to convert.
 *
 * @return The grc.
 */
extern tlog_grc tlog_grc_from(const struct tlog_grc_range *range, int rc);

/**
 * Convert a range rc to a grc.
 *
 * @param \_range_name_token     Range variable name without tlog_grc_ prefix.
 * @param \_rc                   The range rc to convert.
 *
 * @return The grc.
 */
#define TLOG_GRC_FROM(_range_name_token, _rc) \
    tlog_grc_from(&tlog_grc_range_##_range_name_token, _rc)

/** Current value of errno as a grc (not an lvalue) */
#define TLOG_GRC_ERRNO  TLOG_GRC_FROM(errno, errno)

/**
 * Convert a grc to a range rc.
 *
 * @param range The range to convert to.
 * @param grc   The grc to convert.
 *
 * @return The range rc.
 */
extern int tlog_grc_to(const struct tlog_grc_range *range, tlog_grc grc);

/**
 * Convert a grc to a range rc.
 *
 * @param \_range_name_token    Range variable name without tlog_grc_ prefix.
 * @param \_grc                 The grc to convert.
 *
 * @return The range rc.
 */
#define TLOG_GRC_TO(_range_name_token, _grc) \
    tlog_grc_to(&tlog_grc_range_##_range_name_token, _grc)

/**
 * Check if a global return code is valid.
 *
 * @param grc   The global return code to check.
 *
 * @return True if the code is valid, false otherwise.
 */
extern bool tlog_grc_is_valid(tlog_grc grc);

/**
 * Retrieve a global return code description.
 *
 * @param grc   The global return code to retrieve description for.
 *
 * @return Description as a static, constant string.
 */
extern const char *tlog_grc_strerror(tlog_grc grc);

#endif /* _TLOG_GRC_H */
