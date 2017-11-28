/*
 * Tlog-play configuration parsing
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
#include <tlog/play_conf.h>
#include <tlog/conf_origin.h>
#include <tlog/play_conf_cmd.h>
#include <tlog/play_conf_validate.h>
#include <tlog/json_misc.h>
#include <tlog/misc.h>
#include <tlog/rc.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static tlog_grc
tlog_play_conf_file_load(struct tlog_errs **perrs,
                         struct json_object **pconf,
                         const char *path)
{
    tlog_grc grc;
    struct json_object *conf = NULL;

    /* Load the file */
    grc = tlog_json_object_from_file(&conf, path);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushf(perrs, "Failed loading \"%s\"", path);
        goto cleanup;
    }

    /* Validate the contents */
    grc = tlog_play_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_FILE);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushf(perrs, "Invalid contents of \"%s\"", path);
        goto cleanup;
    }

    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;
cleanup:
    json_object_put(conf);
    return grc;
}

tlog_grc
tlog_play_conf_load(struct tlog_errs **perrs,
                    char **pcmd_help, struct json_object **pconf,
                    int argc, char **argv)
{
    tlog_grc grc;
    struct json_object *conf = NULL;
    struct json_object *overlay = NULL;
    char *path = NULL;
    char *cmd_help = NULL;

    assert(pcmd_help != NULL);
    assert(pconf != NULL);
    assert(argv != NULL);

    /* Create empty config */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating configuration object");
        goto cleanup;
    }

    /* Overlay with default config */
    grc = tlog_build_or_inst_path(&path, argv[0],
                                  TLOG_PLAY_CONF_DEFAULT_BUILD_PATH,
                                  TLOG_PLAY_CONF_DEFAULT_INST_PATH);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed finding default configuration");
        goto cleanup;
    }
    grc = tlog_play_conf_file_load(perrs, &overlay, path);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed loading default configuration");
        goto cleanup;
    }
    free(path);
    path = NULL;
    grc = tlog_json_overlay(&conf, conf, overlay);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed overlaying default configuration");
        goto cleanup;
    }
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with local system config */
    grc = tlog_build_or_inst_path(&path, argv[0],
                                  TLOG_PLAY_CONF_LOCAL_BUILD_PATH,
                                  TLOG_PLAY_CONF_LOCAL_INST_PATH);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed finding system configuration");
        goto cleanup;
    }
    grc = tlog_play_conf_file_load(perrs, &overlay, path);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed loading system configuration");
        goto cleanup;
    }
    free(path);
    path = NULL;
    grc = tlog_json_overlay(&conf, conf, overlay);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed overlaying system configuration");
        goto cleanup;
    }
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with command-line config */
    grc = tlog_play_conf_cmd_load(perrs, &cmd_help, &overlay, argc, argv);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs,
                        "Failed retrieving configuration from command line");
        goto cleanup;
    }
    grc = tlog_json_overlay(&conf, conf, overlay);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs,
                        "Failed overlaying command-line configuration");
        goto cleanup;
    }
    json_object_put(overlay);
    overlay = NULL;

    grc = TLOG_RC_OK;
    *pcmd_help = cmd_help;
    cmd_help = NULL;
    *pconf = conf;
    conf = NULL;
cleanup:
    free(cmd_help);
    free(path);
    json_object_put(overlay);
    json_object_put(conf);
    return grc;
}
