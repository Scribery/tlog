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

static const char *tlog_play_conf_cmd_help_fmt =
    "Usage: %1$s [OPTION...]\n"
    "Play back a terminal I/O recording done by tlog-rec.\n"
M4_CONF_CMD_HELP_OPTS()m4_dnl
    "";

M4_CONF_CMD_LOAD_ARGS()m4_dnl

tlog_grc
tlog_play_conf_cmd_load(struct tlog_errs **perrs,
                        char **phelp, struct json_object **pconf,
                        int argc, char **argv)
{
    tlog_grc grc;
    char *progpath = NULL;
    char *progname = NULL;
    char *help = NULL;
    struct json_object *conf = NULL;

    assert(phelp != NULL);
    assert(pconf != NULL);
    assert(argv != NULL);

    /* Create empty configuration */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed creating configuration object");
    }

    /* Extract program name */
    progpath = strdup(argv[0]);
    if (progpath == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed allocating a copy of program path");
    }
    progname = strdup(basename(progpath));
    if (progname == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed allocating program name");
    }

    /* Extract options and positional arguments */
    if (asprintf(&help, tlog_play_conf_cmd_help_fmt, progname) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed formatting help message");
    }
    grc = tlog_play_conf_cmd_load_args(perrs, conf, help, argc, argv);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed extracting configuration "
                         "from options and arguments");
    }

    /* Validate the result */
    grc = tlog_play_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_ARGS);
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
    free(progpath);
    json_object_put(conf);
    return grc;
}
