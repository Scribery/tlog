m4_include(`misc.m4')m4_dnl
m4_include(`conf_cmd.m4')m4_dnl
m4_define(`M4_PROG_NAME', `play')m4_dnl
/*
 * Tlog-play command-line parsing.
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
#include <tlog/play_conf_validate.h>
#include <tlog/play_conf_cmd.h>
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
tlog_play_conf_cmd_load(char **phelp, struct json_object **pconf,
                        int argc, char **argv)
{
    tlog_grc grc;
    char *progpath = NULL;
    char *progname = NULL;
    char *help = NULL;
    struct json_object *conf = NULL;

    /* Create empty configuration */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed creating configuration object: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Extract program name */
    progpath = strdup(argv[0]);
    if (progpath == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed allocating a copy of program path: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    progname = strdup(basename(progpath));
    if (progname == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed allocating program name: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Extract options and positional arguments */
    if (asprintf(&help, tlog_play_conf_cmd_help_fmt, progname) < 0) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed formatting help message: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    grc = tlog_play_conf_cmd_load_args(conf, help, argc, argv);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    /* Validate the result */
    grc = tlog_play_conf_validate(conf, TLOG_CONF_ORIGIN_ARGS);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
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
