/**
 * @file
 * @brief I/O tap handling.
 *
 * A "tap" is an I/O interception setup for an executed program.
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

#ifndef _TLOG_TAP_H
#define _TLOG_TAP_H

#include <tlog/grc.h>
#include <tlog/errs.h>
#include <termios.h>
#include <unistd.h>

/** I/O tap state */
struct tlog_tap {
    pid_t               pid;            /**< Shell PID */
    struct tlog_source *source;         /**< TTY data source */
    struct tlog_sink   *sink;           /**< TTY data sink */
    int                 in_fd;          /**< Shell input FD */
    int                 out_fd;         /**< Shell output FD */
    int                 tty_fd;         /**< Controlling terminal FD, or -1 */
    struct termios      termios_orig;   /**< Original terminal attributes */
    bool                termios_set;    /**< True if terminal attributes were
                                             changed from the original */
};

/** A void I/O tap state initializer */
#define TLOG_TAP_VOID \
    (struct tlog_tap) {      \
        .source = NULL, \
        .sink   = NULL, \
        .in_fd  = -1,   \
        .out_fd = -1,   \
        .tty_fd = -1,   \
    }

/**
 * Setup I/O tap.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param ptap      Location for the tap state.
 * @param euid      The effective UID the program was started with.
 * @param egid      The effective GID the program was started with.
 * @param opts      Execution options: a bitmask of TLOG_EXEC_OPT_* bits.
 * @param path      Path to the recorded program to execute.
 * @param argv      ARGV array for the recorded program.
 * @param in_fd     Stdin to connect to, or -1 if none.
 * @param out_fd    Stdout to connect to, or -1 if none.
 * @param err_fd    Stderr to connect to, or -1 if none.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_tap_setup(struct tlog_errs **perrs,
                               struct tlog_tap *ptap,
                               uid_t euid, gid_t egid,
                               unsigned int opts,
                               const char *path, char **argv,
                               int in_fd, int out_fd, int err_fd);

/**
 * Teardown an I/O tap state.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param tap       The tap state to teardown.
 * @param pstatus   Location for the shell process status as returned by
 *                  waitpid(2), can be NULL, if not required.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_tap_teardown(struct tlog_errs **perrs,
                                  struct tlog_tap *tap,
                                  int *pstatus);

#endif /* _TLOG_TAP_H */
