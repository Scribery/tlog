/**
 * @file
 * @brief Native function return codes (rc's).
 *
 * These are tlog-specific function return codes, which are usually
 * incorporated into global return codes upon use.
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

#ifndef _TLOG_RC_H
#define _TLOG_RC_H

#include <stdbool.h>

/** Minimum function return code */
#define TLOG_RC_MIN  0

/** Function return codes */
typedef enum tlog_rc {
    TLOG_RC_OK = TLOG_RC_MIN,
    TLOG_RC_FAILURE,
    TLOG_RC_JSON_MSG_FIELD_MISSING,
    TLOG_RC_JSON_MSG_FIELD_INVALID_TYPE,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_VER,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_SESSION,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_ID,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_POS,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIME,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT,
    TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN,
    TLOG_RC_FD_JSON_READER_INCOMPLETE_LINE,
    TLOG_RC_JSON_SOURCE_MSG_ID_OUT_OF_ORDER,
    TLOG_RC_JSON_SOURCE_PKT_TS_OUT_OF_ORDER,
    TLOG_RC_JSON_SOURCE_TERMINAL_MISMATCH,
    TLOG_RC_ES_JSON_READER_CURL_INIT_FAILED,
    TLOG_RC_ES_JSON_READER_REPLY_INVALID,
    TLOG_RC_MEM_JSON_READER_INCOMPLETE_LINE,
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
    return rc < TLOG_RC_MAX_PLUS_ONE;
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
