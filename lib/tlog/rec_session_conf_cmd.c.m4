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
    const char *p;
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
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating configuration object");
        goto cleanup;
    }

    /* Extract program name, noting login dash prefix */
    progpath = strdup(argv[0]);
    if (progpath == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed allocating a copy of program path");
        goto cleanup;
    }
    p = basename(progpath);
    if (p[0] == m4_singlequote(-)) {
        p++;
        val = json_object_new_boolean(true);
        if (val == NULL) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating login flag");
            goto cleanup;
        }
        grc = tlog_json_object_object_add_path(conf, "login", val);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed storing login flag");
            goto cleanup;
        }
        val = NULL;
    }
    progname = strdup(p);
    if (progname == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed allocating program name");
        goto cleanup;
    }

    /* Extract options and positional arguments */
    if (asprintf(&help, tlog_rec_session_conf_cmd_help_fmt, progname) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed formatting help message");
        goto cleanup;
    }
    grc = tlog_rec_session_conf_cmd_load_args(perrs, conf, help, argc, argv);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs,
                        "Failed extracting configuration "
                        "from options and arguments");
        goto cleanup;
    }

    /* Validate the result */
    grc = tlog_rec_session_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_ARGS);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Validation of loaded configuration failed");
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
    json_object_put(val);
    json_object_put(conf);
    return grc;
}
