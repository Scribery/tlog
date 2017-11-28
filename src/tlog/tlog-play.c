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
#include <tlog/play.h>
#include <tlog/play_conf.h>
#include <tlog/misc.h>
#include <json.h>
#include <langinfo.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

int
main(int argc, char **argv)
{
    tlog_grc grc;
    struct tlog_errs *errs = NULL;
    struct json_object *conf = NULL;
    char *cmd_help = NULL;
    const char *charset;
    int signal = 0;

    /* Set locale from environment variables */
    if (setlocale(LC_ALL, "") == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs,
                        "Failed setting locale from environment variables");
        goto cleanup;
    }

    /* Read configuration and command-line usage message */
    grc = tlog_play_conf_load(&errs, &cmd_help, &conf, argc, argv);
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

    /* Run */
    grc = tlog_play(&errs, cmd_help, conf, &signal);

cleanup:

    /* Print error stack, if any */
    tlog_errs_print(stderr, errs);

    json_object_put(conf);
    free(cmd_help);
    tlog_errs_destroy(&errs);

    /* Reproduce the exit signal to get proper exit status */
    if (signal != 0) {
        raise(signal);
    }

    return grc != TLOG_RC_OK;
}
