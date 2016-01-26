/**
 * @file
 * @brief TTY terminal data sink.
 *
 * TTY terminal data sink accepts packets and writes them to up to three file
 * descriptors: for input, output and window size changes. Any of them can be
 * omitted. Timestamps are ignored by the sink.
 */
/*
 * Copyright (C) 2016 Red Hat
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

#ifndef _TLOG_TTY_SINK_H
#define _TLOG_TTY_SINK_H

#include <tlog/sink.h>

/**
 * TTY sink type
 *
 * Creation arguments:
 *
 */
extern const struct tlog_sink_type tlog_tty_sink_type;

/**
 * Create (allocate and initialize) a log sink.
 *
 * @param psink     Location for created sink pointer, set to NULL
 *                  in case of error.
 * @param in_fd     File descriptor to write terminal input to,
 *                  or negative number if that's not needed.
 * @param out_fd    File descriptor to write terminal output to,
 *                  or negative number if that's not needed.
 * @param win_fd    File descriptor to write window sizes to,
 *                  or negative number if that's not needed.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_tty_sink_create(struct tlog_sink **psink,
                     int in_fd, int out_fd, int win_fd)
{
    assert(psink != NULL);
    return tlog_sink_create(psink, &tlog_tty_sink_type,
                            in_fd, out_fd, win_fd);
}

#endif /* _TLOG_TTY_SINK_H */
