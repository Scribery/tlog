/**
 * @file
 * @brief UTF-8 filter.
 *
 * UTF-8 filter structure and module are used to separate valid and invalid
 * character byte sequences.
 */
/*
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

#ifndef _TLOG_UTF8_H
#define _TLOG_UTF8_H

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

/** Valid byte value range */
struct tlog_utf8_range {
    uint8_t min;    /**< Lower boundary of the range */
    uint8_t max;    /**< Higher boundary of the range */
};

/** A valid byte sequence */
struct tlog_utf8_seq {
    struct tlog_utf8_range rl[5];   /**< Range list, terminated by
                                         a range with max == 0 */
};

/** List of valid byte sequences, terminated by an empty sequence */
extern const struct tlog_utf8_seq tlog_utf8_seq_list[];

/** UTF-8 filter */
struct tlog_utf8 {
    uint8_t                         buf[4]; /**< Bytes so far */
    const struct tlog_utf8_range   *range;  /**< Next byte value range, refers
                                                 to tlog_utf8_seq_list */
    size_t                          len;    /**< Sequence length so far */
    bool                            end;    /**< True if the sequence has
                                                 ended */
};

/** Empty UTF-8 filter initializer */
#define TLOG_UTF8_INIT (struct tlog_utf8){.len = 0}

/**
 * Check if a UTF-8 filter is valid.
 *
 * @param utf8  The buffer to check.
 *
 * @return True if the buffer is valid, false otherwise.
 */
static inline bool
tlog_utf8_is_valid(const struct tlog_utf8 *utf8)
{
    return utf8->len <= sizeof(utf8->buf) &&
           (utf8->len == 0 || utf8->range != NULL);
}

/**
 * Reset UTF-8 filter.
 *
 * @param utf8  The buffer to reset.
 */
static inline void
tlog_utf8_reset(struct tlog_utf8 *utf8)
{
    assert(utf8 != NULL);
    memset(utf8, 0, sizeof(*utf8));
    assert(tlog_utf8_is_valid(utf8));
}

/**
 * Check if a UTF-8 filter sequence is started (any bytes were added).
 *
 * @param utf8  The filter to check.
 *
 * @return True if the sequence is started, false otherwise.
 */
static inline bool
tlog_utf8_is_started(const struct tlog_utf8 *utf8)
{
    assert(tlog_utf8_is_valid(utf8));
    return utf8->len > 0;
}

/**
 * Check if a UTF-8 filter sequence is ended.
 *
 * @param utf8  The filter to check.
 *
 * @return True if the sequence is ended, false otherwise.
 */
static inline bool
tlog_utf8_is_ended(const struct tlog_utf8 *utf8)
{
    assert(tlog_utf8_is_valid(utf8));
    return utf8->end;
}

/**
 * Check if a UTF-8 filter sequence is complete.
 * Can only be called after the sequence is ended.
 *
 * @param utf8  The filter to check.
 *
 * @return True if the buffer sequence is complete, false otherwise.
 */
static inline bool
tlog_utf8_is_complete(const struct tlog_utf8 *utf8)
{
    assert(tlog_utf8_is_valid(utf8));
    assert(tlog_utf8_is_ended(utf8));
    return utf8->len > 0 && utf8->range->max == 0;
}

/**
 * Check if a UTF-8 filter contains an empty sequence.
 * Can only be called after the sequence is ended.
 *
 * @param utf8  The filter to check.
 *
 * @return True if the filter sequence is empty,
 *         false otherwise.
 */
static inline bool
tlog_utf8_is_empty(const struct tlog_utf8 *utf8)
{
    assert(tlog_utf8_is_valid(utf8));
    assert(tlog_utf8_is_ended(utf8));
    return utf8->len == 0;
}

/**
 * Try to add another byte to a UTF-8 filter.
 * Can't be called after the sequence is ended.
 *
 * @param utf8  The filter to add the byte to.
 * @param b     The byte being added.
 *
 * @return True if the byte was valid and added,
 *         false if it was invalid and not added.
 */
static inline bool
tlog_utf8_add(struct tlog_utf8 *utf8, uint8_t b)
{
    const struct tlog_utf8_range *r;
    assert(tlog_utf8_is_valid(utf8));
    assert(!tlog_utf8_is_ended(utf8));
    assert(utf8->len < sizeof(utf8->buf));

    if (utf8->len == 0) {
        const struct tlog_utf8_seq *seq = tlog_utf8_seq_list;
        while (true) {
            r = seq->rl;
            if (r->max == 0) {
                utf8->end = true;
                assert(tlog_utf8_is_valid(utf8));
                return false;
            }
            if (b >= r->min && b <= r->max)
                break;
            seq++;
        }
    } else {
        r = utf8->range;
        if (b < r->min || b > r->max) {
            utf8->end = true;
            assert(tlog_utf8_is_valid(utf8));
            return false;
        }
    }
    utf8->buf[utf8->len++] = b;
    utf8->range = r + 1;
    if (utf8->range->max == 0)
        utf8->end = true;
    assert(tlog_utf8_is_valid(utf8));
    return true;
}

/**
 * Check if a buffer contents is valid UTF-8 text.
 *
 * @param ptr   Pointer to the buffer to check.
 * @param len   Length of the buffer to check.
 *
 * @return True if the buffer contents is valid UTF-8, false otherwise.
 */
extern bool tlog_utf8_buf_is_valid(const char *ptr, size_t len);

/**
 * Check if a zero-terminated string is valid UTF-8 text.
 *
 * @param str   The string to check.
 *
 * @return True if the string contents is valid UTF-8, false otherwise.
 */
static inline bool
tlog_utf8_str_is_valid(const char *str)
{
    return tlog_utf8_buf_is_valid(str, strlen(str));
}

#endif /* _TLOG_UTF8_H */
