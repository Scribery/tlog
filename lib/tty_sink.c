/*
 * TTY terminal data sink.
 *
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

#include <config.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <tlog/tty_sink.h>

/** TTY sink instance */
struct tlog_tty_sink {
    struct tlog_sink        sink;       /**< Abstract sink instance */
    int                     in_fd;      /**< FD to write input to */
    int                     out_fd;     /**< FD to write output to */
    int                     win_fd;     /**< FD to write window sizes to */
    bool                    got_win;    /**< True if got window size */
    struct winsize          last_win;   /**< Last window size */
};

static bool
tlog_tty_sink_is_valid(const struct tlog_sink *sink)
{
    struct tlog_tty_sink *tty_sink =
                                (struct tlog_tty_sink *)sink;
    return tty_sink != NULL;
}

static void
tlog_tty_sink_cleanup(struct tlog_sink *sink)
{
    struct tlog_tty_sink *tty_sink =
                                (struct tlog_tty_sink *)sink;
    (void) tty_sink; /* unused unless asserts are enabled */

    assert(tty_sink != NULL);
}

static tlog_grc
tlog_tty_sink_init(struct tlog_sink *sink, va_list ap)
{
    struct tlog_tty_sink *tty_sink =
                                (struct tlog_tty_sink *)sink;
    tty_sink->in_fd = va_arg(ap, int);
    tty_sink->out_fd = va_arg(ap, int);
    tty_sink->win_fd = va_arg(ap, int);
    return TLOG_RC_OK;
}

static tlog_grc
tlog_tty_sink_write(struct tlog_sink *sink,
                    const struct tlog_pkt *pkt,
                    struct tlog_pkt_pos *ppos,
                    const struct tlog_pkt_pos *end)
{
    struct tlog_tty_sink *tty_sink =
                                (struct tlog_tty_sink *)sink;

    if (tlog_pkt_pos_cmp(ppos, end) >= 0) {
        return TLOG_RC_OK;
    }

    if (pkt->type == TLOG_PKT_TYPE_WINDOW) {
        if (tty_sink->win_fd >= 0) {
            struct winsize win = {.ws_col = pkt->data.window.width,
                                  .ws_row = pkt->data.window.height};
            if (tty_sink->got_win &&
                win.ws_col == tty_sink->last_win.ws_col &&
                win.ws_row == tty_sink->last_win.ws_row) {
                return TLOG_RC_OK;
            }
            if (ioctl(tty_sink->win_fd, TIOCSWINSZ, &win) < 0) {
                return TLOG_GRC_ERRNO;
            }
            tty_sink->last_win = win;
            tty_sink->got_win = true;
            tlog_pkt_pos_move_past(ppos, pkt);
        }
    } else if (pkt->type == TLOG_PKT_TYPE_IO) {
        int fd = pkt->data.io.output ? tty_sink->out_fd
                                     : tty_sink->in_fd;
        if (fd >= 0) {
            ssize_t rc;
            rc = write(fd, pkt->data.io.buf + ppos->val,
                       end->val - ppos->val);
            if (rc < 0) {
                return TLOG_GRC_ERRNO;
            }
            tlog_pkt_pos_move(ppos, pkt, rc);
        }
    }

    return TLOG_RC_OK;
}

const struct tlog_sink_type tlog_tty_sink_type = {
    .size       = sizeof(struct tlog_tty_sink),
    .init       = tlog_tty_sink_init,
    .cleanup    = tlog_tty_sink_cleanup,
    .is_valid   = tlog_tty_sink_is_valid,
    .write      = tlog_tty_sink_write,
};
