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

#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include "tlog/fd_reader.h"
#include "tlog/source.h"
#include "tlog/rc.h"
#include "tlog/misc.h"

#define BUF_SIZE    4096

/**< Number of the signal causing exit */
static volatile sig_atomic_t exit_signum  = 0;

static void
exit_sighandler(int signum)
{
    if (exit_signum == 0)
        exit_signum = signum;
}

int
main(int argc, char **argv)
{
    const int exit_sig[] = {SIGINT, SIGTERM, SIGHUP};
    struct timespec local_ts;
    struct timespec last_ts;
    struct timespec off_ts;
    tlog_grc grc;
    ssize_t rc;
    size_t i;
    size_t j;
    struct termios orig_termios;
    struct termios raw_termios;
    struct sigaction sa;
    int status = 0;
    struct tlog_reader *reader;
    struct tlog_source *source;
    bool got_pkt = false;
    struct tlog_pkt pkt = TLOG_PKT_VOID;

    (void)argv;
    if (argc > 1) {
        fprintf(stderr, "Arguments are not accepted\n");
        return 1;
    }

    /* Create the reader */
    grc = tlog_fd_reader_create(&reader, STDIN_FILENO, BUF_SIZE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating the reader: %s\n",
                tlog_grc_strerror(grc));
        return 1;
    }

    /* Create the source */
    grc = tlog_source_create(&source, reader, NULL, NULL, 0, BUF_SIZE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating the source: %s\n",
                tlog_grc_strerror(grc));
        return 1;
    }

    /* Get terminal attributes */
    rc = tcgetattr(STDOUT_FILENO, &orig_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed retrieving tty attributes: %s\n",
                strerror(errno));
        return 1;
    }

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN)
        {
            sa.sa_handler = exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < ARRAY_SIZE(exit_sig); j++)
                sigaddset(&sa.sa_mask, exit_sig[j]);
            /* NOTE: no SA_RESTART on purpose */
            sa.sa_flags = 0;
            sigaction(exit_sig[i], &sa, NULL);
        }
    }

    /* Switch the terminal to raw mode */
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    raw_termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                             INPCK | ISTRIP | IXON | PARMRK);
    raw_termios.c_oflag &= ~OPOST;
    raw_termios.c_cc[VMIN] = 1;
    raw_termios.c_cc[VTIME] = 0;
    rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed setting tty attributes: %s\n",
                strerror(errno));
        return 1;
    }

    /*
     * Reproduce the logged output
     */
    while (true) {
        /* Read a packet */
        grc = tlog_source_read(source, &pkt);
        if (grc == TLOG_GRC_FROM(errno, EINTR)) {
            break;
        } else if (grc != TLOG_RC_OK) {
            fprintf(stderr, "Failed reading the source: %s\n",
                    tlog_grc_strerror(grc));
            status = 1;
            break;
        }
        /* If hit the end of stream */
        if (tlog_pkt_is_void(&pkt)) {
            break;
        }
        /* If it's not the output */
        if (pkt.type != TLOG_PKT_TYPE_IO || !pkt.data.io.output) {
            tlog_pkt_cleanup(&pkt);
            continue;
        }

        /* If it's the first packet */
        if (!got_pkt) {
            if (clock_gettime(CLOCK_MONOTONIC, &local_ts) != 0) {
                fprintf(stderr, "Failed retrieving current time: %s\n",
                        strerror(rc));
                status = 1;
                break;
            }
            got_pkt = true;
        /* Else, if we need a delay from the previous packet */
        } else if (tlog_timespec_cmp(&pkt.timestamp, &last_ts) > 0) {
            tlog_timespec_sub(&pkt.timestamp, &last_ts, &off_ts);
            tlog_timespec_add(&local_ts, &off_ts, &local_ts);
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                 &local_ts, NULL);
            if (rc == EINTR) {
                break;
            } else if (rc != 0) {
                fprintf(stderr, "Failed sleeping: %s\n", strerror(rc));
                status = 1;
                break;
            }
        }

        /* Output the packet */
        if (write(STDOUT_FILENO, pkt.data.io.buf, pkt.data.io.len) !=
                (ssize_t)pkt.data.io.len) {
            if (errno == EINTR) {
                break;
            } else if (rc != 0) {
                fprintf(stderr, "Failed writing output: %s\n", strerror(rc));
                status = 1;
                break;
            }
        }

        last_ts = pkt.timestamp;
        tlog_pkt_cleanup(&pkt);
    }

    /* Restore signal handlers */
    for (i = 0; i < ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN)
            signal(exit_sig[i], SIG_DFL);
    }

    /* Restore terminal attributes */
    rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &orig_termios);
    if (rc < 0 && errno != EBADF) {
        fprintf(stderr, "Failed restoring tty attributes: %s\n",
                strerror(errno));
        return 1;
    }

    /* Reproduce the exit signal to get proper exit status */
    if (exit_signum != 0)
        raise(exit_signum);

    return status;
}
