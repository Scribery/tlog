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
tlog_play_conf_file_load(struct json_object **pconf,
                         const char *path)
{
    tlog_grc grc;
    struct json_object *conf = NULL;

    /* Load the file */
    grc = tlog_json_object_from_file(&conf, path);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed loading \"%s\": %s\n",
                path, tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Validate the contents */
    grc = tlog_play_conf_validate(conf, TLOG_CONF_ORIGIN_FILE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Invalid contents of \"%s\": %s\n",
                path, tlog_grc_strerror(grc));
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
tlog_play_conf_load(char **pprogname, struct json_object **pconf,
                    int argc, char **argv)
{
    tlog_grc grc;
    struct json_object *conf = NULL;
    struct json_object *overlay = NULL;
    char *path = NULL;
    char *progname = NULL;

#define GUARD(_expr) \
    do {                            \
        grc = (_expr);              \
        if (grc != TLOG_RC_OK) {    \
            goto cleanup;           \
        }                           \
    } while (0)

    /* Create empty config */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed creating configuration object: %s",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Overlay with default config */
    GUARD(tlog_build_or_inst_path(&path, argv[0],
                                  TLOG_PLAY_CONF_DEFAULT_BUILD_PATH,
                                  TLOG_PLAY_CONF_DEFAULT_INST_PATH));
    GUARD(tlog_play_conf_file_load(&overlay, path));
    free(path);
    path = NULL;
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with local system config */
    GUARD(tlog_build_or_inst_path(&path, argv[0],
                                  TLOG_PLAY_CONF_LOCAL_BUILD_PATH,
                                  TLOG_PLAY_CONF_LOCAL_INST_PATH));
    GUARD(tlog_play_conf_file_load(&overlay, path));
    free(path);
    path = NULL;
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with command-line config */
    GUARD(tlog_play_conf_cmd_load(&progname, &overlay, argc, argv));
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

#undef GUARD

    grc = TLOG_RC_OK;
    *pprogname = progname;
    progname = NULL;
    *pconf = conf;
    conf = NULL;
cleanup:
    free(progname);
    free(path);
    json_object_put(overlay);
    json_object_put(conf);
    return grc;
}
