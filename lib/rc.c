/*
 * Tlog function return codes.
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

#include <assert.h>
#include <stdlib.h>
#include <tlog/rc.h>

static const char *tlog_rc_desc_list[TLOG_RC_MAX_PLUS_ONE - TLOG_RC_MIN] = {
    [TLOG_RC_OK] =
        "Success",
    [TLOG_RC_FAILURE] =
        "Failure",
    [TLOG_RC_MSG_FIELD_MISSING] =
        "A message field is missing",
    [TLOG_RC_MSG_FIELD_INVALID_TYPE] =
        "A message field has invalid type",
    [TLOG_RC_MSG_FIELD_INVALID_VALUE] =
        "A message field has invalid value",
    [TLOG_RC_FD_READER_INCOMPLETE_LINE] =
        "Incomplete message object line encountered",
    [TLOG_RC_SOURCE_MSG_ID_OUT_OF_ORDER] =
        "Message ID is out of order",
    [TLOG_RC_SOURCE_PKT_TS_OUT_OF_ORDER] =
        "Packet timestamp is out of order",
    [TLOG_RC_ES_READER_CURL_INIT_FAILED] =
        "Curl handle creation failed",
    [TLOG_RC_ES_READER_REPLY_INVALID] =
        "Invalid reply received from HTTP server",
};

const char *
tlog_rc_strerror(tlog_rc rc)
{
    const char *desc;
    assert(tlog_rc_is_valid(rc));

    desc = tlog_rc_desc_list[rc];
    assert(desc != NULL);
    return desc ? desc : "Unknown return code";
}
