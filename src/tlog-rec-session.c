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

#include <config.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include <langinfo.h>
#include <tlog/rc.h>
#include <tlog/rec.h>
#include <tlog/rec_session_conf.h>
#include <tlog/misc.h>

/**
 * Let GNU TLS know we don't need implicit global initialization, which opens
 * /dev/urandom and can replace standard FDs in case they were closed.
 */
int
_gnutls_global_init_skip(void)
{
    return 1;
}

int
main(int argc, char **argv)
{
    tlog_grc grc;
    uid_t euid;
    gid_t egid;
    struct tlog_errs *errs = NULL;
    struct json_object *conf = NULL;
    char *cmd_help = NULL;
    int std_fds[] = {0, 1, 2};
    const char *charset;
    const char *shell_path;
    char **shell_argv = NULL;
    int status = 1;
    int signal = 0;

    /* Remember effective UID/GID so we can return to them later */
    euid = geteuid();
    egid = getegid();

    /* Drop the privileges temporarily in case we're SUID/SGID */
    if (seteuid(getuid()) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs, "Failed setting EUID");
        goto cleanup;
    }
    if (setegid(getgid()) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs, "Failed setting EGID");
        goto cleanup;
    }

    /* Check if stdin/stdout/stderr are closed and stub them with /dev/null */
    /* NOTE: Must be done first to avoid FD takeover by other code */
    while (true) {
        int fd = open("/dev/null", O_RDWR);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(&errs, grc);
            tlog_errs_pushs(&errs, "Failed opening /dev/null");
            goto cleanup;
        } else if (fd >= (int)TLOG_ARRAY_SIZE(std_fds)) {
            close(fd);
            break;
        } else {
            std_fds[fd] = -1;
        }
    }

    /* Set locale from environment variables */
    if (setlocale(LC_ALL, "") == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs,
                        "Failed setting locale from environment variables");
        goto cleanup;
    }

    /* Read configuration and command-line usage message */
    grc = tlog_rec_session_conf_load(&errs, &cmd_help, &conf, argc, argv);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    /* Check that the character encoding is supported */
    charset = nl_langinfo(CODESET);
    if (strcmp(charset, "UTF-8") != 0) {
        if (strcmp(charset, "ANSI_X3.4-1968") == 0) {
            tlog_errs_pushf(&errs, "Locale charset is ANSI_X3.4-1968 (ASCII)");
            tlog_errs_pushf(&errs, "Assuming locale environment is lost "
                                   "and charset is UTF-8");
        } else {
            grc = TLOG_RC_FAILURE;
            tlog_errs_pushf(&errs, "Unsupported locale charset: %s", charset);
            goto cleanup;
        }
    }

    /* Prepare shell command line */
    grc = tlog_rec_session_conf_get_shell(&errs, conf,
                                          &shell_path, &shell_argv);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(&errs, "Failed building shell command line");
        goto cleanup;
    }

    /*
     * Set the SHELL environment variable to the actual shell. Otherwise
     * programs trying to spawn the user's shell would start tlog-rec-session
     * instead.
     */
    if (setenv("SHELL", shell_path, /*overwrite=*/1) != 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs, "Failed to set SHELL environment variable");
        goto cleanup;
    }

    /* Run */
    grc = tlog_rec(&errs, euid, egid, cmd_help, conf,
                   TLOG_REC_OPT_LOCK_SESS | TLOG_REC_OPT_DROP_PRIVS,
                   shell_path, shell_argv,
                   std_fds[0], std_fds[1], std_fds[2],
                   &status, &signal);

cleanup:

    /* Print error stack, if any */
    tlog_errs_print(stderr, errs);

    /* Free shell argv list */
    if (shell_argv != NULL) {
        size_t i;
        for (i = 0; shell_argv[i] != NULL; i++) {
            free(shell_argv[i]);
        }
        free(shell_argv);
    }

    json_object_put(conf);
    free(cmd_help);
    tlog_errs_destroy(&errs);

    /* Reproduce the exit signal to get proper exit status */
    if (signal != 0) {
        raise(signal);
    }

    /* Signal error if tlog error occurred */
    if (grc != TLOG_RC_OK) {
        return 1;
    }

    /* Mimick Bash exit status generation */
    return WIFSIGNALED(status)
                ? 128 + WTERMSIG(status)
                : WEXITSTATUS(status);
}
