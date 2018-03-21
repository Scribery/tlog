m4_include(`misc.m4')m4_dnl
m4_include(`conf_cmd.m4')m4_dnl
m4_changecom()m4_dnl
/*
 * Tlog-M4_PROG_NAME() command-line parsing.
 *
m4_generated_warning(` * ')m4_dnl
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
#include <tlog/M4_PROG_SYM()_conf_validate.h>
#include <tlog/M4_PROG_SYM()_conf_cmd.h>
#include <tlog/json_misc.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

static const char *tlog_rec_session_conf_cmd_help_fmt =
    "Usage: %1$s [OPTION...] [CMD_FILE [CMD_ARG...]]\n"
    "   or: %1$s -c [OPTION...] CMD_STRING [CMD_NAME [CMD_ARG...]]\n"
    "Start a shell and log terminal I/O.\n"
M4_CONF_CMD_HELP_OPTS()m4_dnl
    "";

M4_CONF_CMD_LOAD_ARGS()m4_dnl

tlog_grc
tlog_rec_session_conf_cmd_load(struct tlog_errs **perrs,
                               char **phelp, struct json_object **pconf,
                               int argc, char **argv)
{
    tlog_grc grc;
    char *progpath = NULL;
    const char *shell_suffix = "-shell-";
    const char *progname_start;
    const char *progname_end;
    const char *shell_start;
    char *shell = NULL;
    char *src;
    char *dst;
    char *progname = NULL;
    char *help = NULL;
    struct json_object *conf = NULL;
    struct json_object *val = NULL;

    assert(phelp != NULL);
    assert(pconf != NULL);
    assert(argv != NULL);

    /* Create empty configuration */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed creating configuration object");
    }

    /* Get program path */
    progpath = strdup(argv[0]);
    if (progpath == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed allocating a copy of program path");
    }

    /* Check for and extract login dash prefix */
    progname_start = basename(progpath);
    if (progname_start[0] == '-') {
        progname_start++;
        val = json_object_new_boolean(true);
        if (val == NULL) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed creating login flag");
        }
        grc = tlog_json_object_object_add_path(conf, "login", val);
        if (grc != TLOG_RC_OK) {
            TLOG_ERRS_RAISECS(grc, "Failed storing login flag");
        }
        val = NULL;
    }

    /* Check for the shell suffix and extract the shell */
    progname_end = strstr(progname_start, shell_suffix);
    if (progname_end == NULL) {
        progname_end = progname_start + strlen(progname_start);
    } else {
        /* Copy the encoded shell path */
        assert(strlen(shell_suffix) > 0);
        assert(shell_suffix[strlen(shell_suffix) - 1] == '-');
        shell_start = progname_end + strlen(shell_suffix) - 1;
        shell = strdup(shell_start);
        if (shell == NULL) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc,
                              "Failed allocating a copy of the shell part "
                              "of the program name");
        }
        /* Decode and unescape the shell path */
        for (dst = src = shell; ;) {
            if (*src == '-') {
                *dst = '/';
                continue;
            } else if (*src == '\\') {
                src++;
            }
            if ((*dst++ = *src++) == '\0') {
                break;
            }
        }
        /* Store the shell */
        val = json_object_new_string(shell);
        if (val == NULL) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed creating shell path object");
        }
        grc = tlog_json_object_object_add_path(conf, "shell", val);
        if (grc != TLOG_RC_OK) {
            TLOG_ERRS_RAISECS(grc, "Failed storing shell path");
        }
        val = NULL;
    }

    /* Extract the program name */
    progname = strndup(progname_start, progname_end - progname_start);
    if (progname == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed allocating program name");
    }

    /* Extract options and positional arguments */
    if (asprintf(&help, tlog_rec_session_conf_cmd_help_fmt, progname) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed formatting help message");
    }
    grc = tlog_rec_session_conf_cmd_load_args(perrs, conf, help, argc, argv);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed extracting configuration "
                         "from options and arguments");
    }

    /* Validate the result */
    grc = tlog_rec_session_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_ARGS);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Validation of loaded configuration failed");
    }

    *phelp = help;
    help = NULL;
    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;

cleanup:
    free(help);
    free(progname);
    free(shell);
    free(progpath);
    json_object_put(val);
    json_object_put(conf);
    return grc;
}
