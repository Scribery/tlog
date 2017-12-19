/**
 * @file
 * @brief Generic recording process
 */
/*
 * Copyright (C) 2015-2017 Red Hat
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

#ifndef _TLOG_REC_H
#define _TLOG_REC_H

#include <tlog/grc.h>
#include <tlog/errs.h>
#include <tlog/misc.h>
#include <json.h>
#include <sys/types.h>

/** tlog_rec option bits (must be ascending powers of two) */
enum tlog_rec_opt {
    /** Recording option bit: drop privileges */
    TLOG_REC_OPT_DROP_PRIVS     = TLOG_EXEC_OPT_DROP_PRIVS,
    /** Recording option bit: search for program in PATH */
    TLOG_REC_OPT_SEARCH_PATH    = TLOG_EXEC_OPT_SEARCH_PATH,
    /** Recording option bit: enable session locking */
    TLOG_REC_OPT_LOCK_SESS      = 0x04,
    /** Maximum option bit value plus one (not a valid bit) */
    TLOG_REC_OPT_MAX_PLUS_ONE
};

/** Bitmask with all tlog_rec_opt bits on */
#define TLOG_REC_OPT_MASK   (((TLOG_REC_OPT_MAX_PLUS_ONE - 1) << 1) - 1)

/**
 * Run recording with specified configuration and environment.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param euid      The effective UID the program was started with.
 * @param egid      The effective GID the program was started with.
 * @param cmd_help  Command-line usage help message.
 * @param conf      Recording program configuration JSON object.
 * @param opts      A bitmask of TLOG_REC_OPT_* bits.
 * @param path      Path to the recorded program to execute.
 * @param argv      ARGV array for the recorded program.
 * @param in_fd     Stdin to connect to, or -1 if none.
 * @param out_fd    Stdout to connect to, or -1 if none.
 * @param err_fd    Stderr to connect to, or -1 if none.
 * @param pstatus   Location for the recorded program exit status as returned
 *                  by waitpid(2). Not modified in case of error. Can be NULL,
 *                  if not needed.
 * @param psignal   Location for the number of signal which caused recording
 *                  termination, or for zero, if terminated for other reason.
 *                  Not modified in case of error. Can be NULL, if not needed.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec(struct tlog_errs **perrs, uid_t euid, gid_t egid,
                         const char *cmd_help,
                         struct json_object *conf, unsigned int opts,
                         const char *path, char **argv,
                         int in_fd, int out_fd, int err_fd,
                         int *pstatus, int *psignal);

#endif /* _TLOG_REC_H */
