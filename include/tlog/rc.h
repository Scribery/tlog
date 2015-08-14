/*
 * Tlog-native function return codes (rc's).
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

#ifndef _TLOG_RC_H
#define _TLOG_RC_H

#include <stdbool.h>

/** Minimum function return code */
#define TLOG_RC_MIN  0

/** Function return codes */
typedef enum tlog_rc {
    TLOG_RC_OK = TLOG_RC_MIN,
    TLOG_RC_FAILURE,
    TLOG_RC_MSG_FIELD_MISSING,
    TLOG_RC_MSG_FIELD_INVALID_TYPE,
    TLOG_RC_MSG_FIELD_INVALID_VALUE,
    TLOG_RC_FD_READER_INCOMPLETE_LINE,
    /* Return code upper boundary (not a valid return code) */
    TLOG_RC_MAX_PLUS_ONE
} tlog_rc;

/**
 * Check if an rc is valid.
 *
 * @param rc    The rc to check.
 *
 * @return True if the rc is valid, false otherwise.
 */
static inline bool
tlog_rc_is_valid(tlog_rc rc)
{
    return (rc >= TLOG_RC_MIN && rc < TLOG_RC_MAX_PLUS_ONE);
}

/**
 * Retrieve a global return code description.
 *
 * @param rc    The global return code to retrieve description for.
 *
 * @return Description as a static, constant string.
 */
extern const char *tlog_rc_strerror(tlog_rc rc);

#endif /* _TLOG_RC_H */
