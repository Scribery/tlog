/**
 * @file
 * @brief Rate-limiting JSON message writer.
 *
 * Rate-limiting JSON writer passes messages written to it to a "below"
 * writer, calculating and limiting their rate and "burstiness". Each written
 * message size is compared to the specified rate and burst threshold. If it
 * passes, then the message is written to the "below" writer. If it fails the
 * check, then, depending on creation parameters, it is either discarded and
 * the writer reports success, or the write to the "below" writer is delayed
 * until the message conforms to the rate and the burst threshold.
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

#ifndef _TLOG_RL_JSON_WRITER_H
#define _TLOG_RL_JSON_WRITER_H

#include <assert.h>
#include <tlog/json_writer.h>

/** Rate-limiting JSON message writer type */
extern const struct tlog_json_writer_type tlog_rl_json_writer_type;

/**
 * Create an instance of rate-limiting writer.
 *
 * @param pwriter       Location for the pointer to the created writer.
 * @param below         The "below" writer to write messages to.
 * @param below_owned   True if the "below" writer should be destroyed when
 *                      the created rate limit writer is destroyed.
 * @param clock_id      ID of the clock to use to calculate message rate and
 *                      to delay messages.
 * @param rate          Average message data rate, bytes per second.
 * @param burst         Maximum message burst size, bytes.
 * @param drop          False if writing a message exceeding maximum rate
 *                      should be delayed until it fits, true if the message
 *                      should be discarded instead.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_rl_json_writer_create(struct tlog_json_writer **pwriter,
                           struct tlog_json_writer *below, bool below_owned,
                           clockid_t clock_id,
                           size_t rate, size_t burst, bool drop)
{
    assert(pwriter != NULL);
    assert(tlog_json_writer_is_valid(below));
    return tlog_json_writer_create(pwriter, &tlog_rl_json_writer_type,
                                   below, below_owned, clock_id,
                                   rate, burst, drop);
}

#endif /* _TLOG_RL_JSON_WRITER_H */
