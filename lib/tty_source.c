/*
 * TTY terminal data source.
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
#include <tlog/timespec.h>
#include <tlog/misc.h>
#include <tlog/tty_source.h>

/** Number of WINCH signals caught */
static volatile sig_atomic_t tlog_tty_source_sig = 0;

/** SIGWINCH handler */
static void
tlog_tty_source_winch_sighandler(int signum)
{
    (void)signum;
    tlog_tty_source_sig++;
}

/** FD index */
enum tlog_tty_source_fd_idx {
    TLOG_TTY_SOURCE_FD_IDX_IN,
    TLOG_TTY_SOURCE_FD_IDX_OUT,
    TLOG_TTY_SOURCE_FD_IDX_NUM
};

/** TTY source instance */
struct tlog_tty_source {
    struct tlog_source      source;     /**< Abstract source instance */
    clockid_t               clock_id;   /**< Clock to use for timestamps */
    struct sigaction        orig_sa;    /**< Original SIGWINCH sigaction */
    size_t                  io_size;    /**< Size of I/O buffer */
    uint8_t                *io_buf;     /**< Pointer to I/O buffer */
    struct pollfd           fd_list[TLOG_TTY_SOURCE_FD_IDX_NUM];
    size_t                  fd_idx;     /**< Index of FD read last */
    int                     win_fd;     /**< Window size source FD */
    bool                    started;    /**< True if read */
    struct timespec         start_ts;   /**< First read timestamp */
    sig_atomic_t            last_sig;   /**< Last SIGWINCH number */
    struct winsize          last_win;   /**< Last window size */
};

static bool
tlog_tty_source_is_valid(const struct tlog_source *source)
{
    struct tlog_tty_source *tty_source =
                                (struct tlog_tty_source *)source;
    return tty_source != NULL &&
           tty_source->io_size >= TLOG_TTY_SOURCE_IO_SIZE_MIN &&
           tty_source->io_buf != NULL &&
           tty_source->fd_list[TLOG_TTY_SOURCE_FD_IDX_IN].events == POLLIN &&
           tty_source->fd_list[TLOG_TTY_SOURCE_FD_IDX_OUT].events == POLLIN;
}

static void
tlog_tty_source_cleanup(struct tlog_source *source)
{
    struct tlog_tty_source *tty_source =
                                (struct tlog_tty_source *)source;
    assert(tty_source != NULL);
    if (tty_source->win_fd >= 0) {
        /* Restore WINCH sigaction */
        sigaction(SIGWINCH, &tty_source->orig_sa, NULL);
        /* Leave the duty of window size retrieval */
        tty_source->win_fd = -1;
    }
    free(tty_source->io_buf);
    tty_source->io_buf = NULL;
}

static tlog_grc
tlog_tty_source_init(struct tlog_source *source, va_list ap)
{
    struct tlog_tty_source *tty_source =
                                (struct tlog_tty_source *)source;
    tlog_grc grc;
    int in_fd = va_arg(ap, int);
    int out_fd = va_arg(ap, int);
    int win_fd = va_arg(ap, int);
    int io_size = va_arg(ap, size_t);
    clockid_t clock_id = va_arg(ap, clockid_t);

    assert(io_size >= TLOG_TTY_SOURCE_IO_SIZE_MIN);

    tty_source->fd_list[TLOG_TTY_SOURCE_FD_IDX_IN].fd = in_fd;
    tty_source->fd_list[TLOG_TTY_SOURCE_FD_IDX_IN].events = POLLIN;
    tty_source->fd_list[TLOG_TTY_SOURCE_FD_IDX_OUT].fd = out_fd;
    tty_source->fd_list[TLOG_TTY_SOURCE_FD_IDX_OUT].events = POLLIN;

    /* Non-existing FD was read last */
    tty_source->fd_idx = SIZE_MAX;

    /* Don't commit to getting window sizes yet */
    tty_source->win_fd = -1;

    tty_source->clock_id = clock_id;
    tty_source->io_size = io_size;
    tty_source->io_buf = malloc(io_size);
    if (tty_source->io_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    /* If asked to transfer window size changes */
    if (win_fd >= 0) {
        struct sigaction sa;
        /* Setup WINCH signal handler */
        sa.sa_handler = tlog_tty_source_winch_sighandler;
        sigemptyset(&sa.sa_mask);
        /* NOTE: no SA_RESTART on purpose */
        sa.sa_flags = 0;
        if (sigaction(SIGWINCH, &sa, &tty_source->orig_sa) < 0) {
            grc = TLOG_GRC_ERRNO;
            goto error;
        }
        /* Accept the duty and mark sigaction saved */
        tty_source->win_fd = win_fd;
    }

    return TLOG_RC_OK;
error:
    tlog_tty_source_cleanup(source);
    return grc;
}

static size_t
tlog_tty_source_loc_get(const struct tlog_source *source)
{
    struct tlog_tty_source *tty_source =
                                (struct tlog_tty_source *)source;
    if (!tty_source->started) {
        return 0;
    } else {
        struct timespec ts;
        clock_gettime(tty_source->clock_id, &ts);
        tlog_timespec_sub(&ts, &tty_source->start_ts, &ts);
        return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
}

static char *
tlog_tty_source_loc_fmt(const struct tlog_source *source, size_t loc)
{
    char *str;
    (void)source;
    if (asprintf(&str, "%zums", loc) < 0) {
        return NULL;
    }
    return str;
}

/**
 * Call poll(2) if at least one FD in the poll FD list is "active"
 * (i.e. non-negative), and poll(2) will be able to return an event.
 *
 * @param fds       FD list pointer.
 * @param nfds      Number of FDs in the list.
 * @param timeout   Number of milliseconds to block for, negative means
 *                  infinity.
 *
 * @return Number of FDs that got events, or -1 on error.
 */
static int
tlog_tty_source_poll_if_active(struct pollfd *fds, nfds_t nfds, int timeout)
{
    nfds_t i;
    for (i = 0; i < nfds; i++) {
        if (fds[i].fd >= 0) {
            return poll(fds, nfds, timeout);
        }
    }
    return 0;
}

static tlog_grc
tlog_tty_source_read(struct tlog_source *source, struct tlog_pkt *pkt)
{
    struct tlog_tty_source *tty_source =
                                (struct tlog_tty_source *)source;
    struct timespec ts;

    assert(tlog_pkt_is_void(pkt));

    /* If asked to transfer window size changes */
    if (tty_source->win_fd >= 0) {
        sig_atomic_t sig = tlog_tty_source_sig;

        /* If this is the first read, or received SIGWINCH */
        if (!tty_source->started || sig != tty_source->last_sig) {
            struct winsize win;

            /* Retrieve window size */
            if (ioctl(tty_source->win_fd, TIOCGWINSZ, &win) < 0) {
                return TLOG_GRC_ERRNO;
            }

            /* If this is the first read, or window size has changed */
            if (!tty_source->started ||
                win.ws_row != tty_source->last_win.ws_row ||
                win.ws_col != tty_source->last_win.ws_col) {
                /* Retrieve timestamp */
                if (clock_gettime(tty_source->clock_id, &ts) < 0) {
                    return TLOG_GRC_ERRNO;
                }
                tlog_pkt_init_window(pkt, &ts, win.ws_col, win.ws_row);
                /* Remember last window */
                tty_source->last_win = win;
            }

            /* Mark SIGWINCH processed */
            tty_source->last_sig = sig;

            /* If got something to return */
            if (!tlog_pkt_is_void(pkt)) {
                goto success;
            }
        }
    }

    /* Wait for I/O until interrupted by SIGWINCH or other signal */
    if (tlog_tty_source_poll_if_active(tty_source->fd_list,
                                       TLOG_ARRAY_SIZE(tty_source->fd_list),
                                       -1) < 0) {
            return TLOG_GRC_ERRNO;
    }

    /*
     * Read I/O.
     */
    while (true) {
        /* Make sure we start from another FD each call */
        tty_source->fd_idx++;
        if (tty_source->fd_idx >= TLOG_ARRAY_SIZE(tty_source->fd_list)) {
            tty_source->fd_idx = 0;
        }
        if (tty_source->fd_list[tty_source->fd_idx].revents &
                (POLLIN | POLLHUP | POLLERR)) {
            ssize_t rc;

            rc = read(tty_source->fd_list[tty_source->fd_idx].fd,
                      tty_source->io_buf, tty_source->io_size);

            if (rc < 0) {
                return TLOG_GRC_ERRNO;
            } else if (rc > 0) {
                if (clock_gettime(tty_source->clock_id, &ts) < 0) {
                    return TLOG_GRC_ERRNO;
                }
                tlog_pkt_init_io(pkt, &ts,
                                 tty_source->fd_idx ==
                                    TLOG_TTY_SOURCE_FD_IDX_OUT,
                                 tty_source->io_buf, false, rc);
            }
            goto success;
        }
    }

success:
    if (!tlog_pkt_is_void(pkt)) {
        if (!tty_source->started) {
            tty_source->start_ts = pkt->timestamp;
            tty_source->started = true;
        }
    }

    return TLOG_RC_OK;
}

const struct tlog_source_type tlog_tty_source_type = {
    .size       = sizeof(struct tlog_tty_source),
    .init       = tlog_tty_source_init,
    .cleanup    = tlog_tty_source_cleanup,
    .is_valid   = tlog_tty_source_is_valid,
    .read       = tlog_tty_source_read,
    .loc_get    = tlog_tty_source_loc_get,
    .loc_fmt    = tlog_tty_source_loc_fmt,
};
