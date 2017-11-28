/*
 * Tlog-rec configuration parsing
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
#include <tlog/rec_conf.h>
#include <tlog/conf_origin.h>
#include <tlog/rec_conf_cmd.h>
#include <tlog/rec_conf_validate.h>
#include <tlog/json_misc.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static tlog_grc
tlog_rec_conf_str_parse(struct tlog_errs **perrs,
                        struct json_object **pconf,
                        const char *str,
                        enum tlog_conf_origin origin)
{
    struct json_object *conf = NULL;
    enum json_tokener_error jerr;
    tlog_grc grc = TLOG_RC_FAILURE;

    assert(pconf != NULL);
    assert(str != NULL);

    errno = 0;
    conf = json_tokener_parse_verbose(str, &jerr);
    if (conf == NULL) {
        grc = TLOG_GRC_FROM(json, jerr);
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed parsing the configuration string");
        goto cleanup;
    }

    grc = tlog_rec_conf_validate(perrs, conf, origin);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs,
                        "Failed validating configuration from the string");
        goto cleanup;
    }

    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;
cleanup:
    json_object_put(conf);
    return grc;
}

static tlog_grc
tlog_rec_conf_file_load(struct tlog_errs **perrs,
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
    grc = tlog_rec_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_FILE);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushf(perrs,
                        "Failed validating contents of \"%s\"", path);
        goto cleanup;
    }

    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;
cleanup:
    json_object_put(conf);
    return grc;
}

static tlog_grc
tlog_rec_conf_env_load(struct tlog_errs **perrs, struct json_object **pconf)
{
    tlog_grc grc;
    const char *val;
    struct json_object *conf = NULL;
    struct json_object *overlay = NULL;

    assert(pconf != NULL);

    /* Create empty config */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating configuration object");
        goto cleanup;
    }

    /* Load the config file, if specified */
    val = getenv("TLOG_REC_CONF_FILE");
    if (val != NULL) {
        grc = tlog_rec_conf_file_load(perrs, &overlay, val);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushs(perrs,
                            "Failed loading the file referenced by "
                            "TLOG_REC_CONF_FILE environment variable");
            goto cleanup;
        }
        grc = tlog_json_overlay(&conf, conf, overlay);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed overlaying configuration");
            goto cleanup;
        }
        json_object_put(overlay);
        overlay = NULL;
    }

    /* Load the config text, if specified */
    val = getenv("TLOG_REC_CONF_TEXT");
    if (val != NULL) {
        grc = tlog_rec_conf_str_parse(perrs, &overlay, val, TLOG_CONF_ORIGIN_ENV);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushs(perrs,
                            "Failed parsing the contents of "
                            "TLOG_REC_CONF_TEXT environment variable");
            goto cleanup;
        }
        grc = tlog_json_overlay(&conf, conf, overlay);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed overlaying configuration");
            goto cleanup;
        }
        json_object_put(overlay);
        overlay = NULL;
    }

    /* Validate the config */
    grc = tlog_rec_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_ENV);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushf(perrs,
                        "Failed validating environment configuration");
        goto cleanup;
    }

    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;
cleanup:
    json_object_put(overlay);
    json_object_put(conf);
    return grc;
}

tlog_grc
tlog_rec_conf_load(struct tlog_errs **perrs,
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
        tlog_errs_pushs(perrs, "Failed creating configuration object: %s");
        goto cleanup;
    }

    /* Overlay with default config */
    grc = tlog_build_or_inst_path(&path, argv[0],
                                  TLOG_REC_CONF_DEFAULT_BUILD_PATH,
                                  TLOG_REC_CONF_DEFAULT_INST_PATH);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed finding default configuration");
        goto cleanup;
    }
    grc = tlog_rec_conf_file_load(perrs, &overlay, path);
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
                                  TLOG_REC_CONF_LOCAL_BUILD_PATH,
                                  TLOG_REC_CONF_LOCAL_INST_PATH);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed finding system configuration");
        goto cleanup;
    }
    grc = tlog_rec_conf_file_load(perrs, &overlay, path);
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

    /* Overlay with environment config */
    grc = tlog_rec_conf_env_load(perrs, &overlay);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs,
                        "Failed retrieving configuration "
                        "from environment variables");
        goto cleanup;
    }
    grc = tlog_json_overlay(&conf, conf, overlay);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs,
                        "Failed overlaying configuration "
                        "retrieved from environment variables");
        goto cleanup;
    }
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with command-line config */
    grc = tlog_rec_conf_cmd_load(perrs, &cmd_help, &overlay, argc, argv);
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

tlog_grc
tlog_rec_conf_get_prog(struct tlog_errs **perrs,
                       struct json_object *conf,
                       const char **ppath,
                       char ***pargv)
{
    tlog_grc grc;
    struct json_object *args;
    size_t argc;
    const char *path;
    char **argv = NULL;
    size_t argi = 0;
    struct json_object *obj;
    const char *arg;
    char *arg_copy = NULL;

    /* Read the positional arguments */
    if (!json_object_object_get_ex(conf, "args", &args)) {
        tlog_errs_pushs(perrs, "Positional arguments are not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    argc = json_object_array_length(args);

    /* If we have positional arguments */
    if (argc > 0) {
        /* Extract the first argument */
        obj = json_object_array_get_idx(args, argi);
        arg = json_object_get_string(obj);
    } else {
        /* Try to use shell from environment as the first argument */
        arg = getenv("SHELL");
        /* If it's not set */
        if (arg == NULL) {
            struct passwd *passwd;
            /* Use user shell from libc as the first argument */
            passwd = getpwuid(getuid());
            if (passwd == NULL) {
                if (errno == 0) {
                    grc = TLOG_RC_FAILURE;
                    tlog_errs_pushs(perrs, "User entry not found");
                } else {
                    grc = TLOG_GRC_ERRNO;
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs, "Failed retrieving user entry");
                }
                goto cleanup;
            }
            arg = passwd->pw_shell;
        }
        argc = 1;
    }

    /* Set first argument as the program path */
    path = arg;

    /* Allocate ARGV array */
    argv = calloc(argc + 1, sizeof(*argv));
    if (argv == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs,
                        "Failed allocating recorded program ARGV array");
        goto cleanup;
    }

    /* Copy the arguments to the ARGV array */
    while (true) {
        arg_copy = strdup(arg);
        if (arg_copy == NULL) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs,
                            "Failed allocating recorded program argv[#%zu]",
                            argi);
            goto cleanup;
        }
        argv[argi] = arg_copy;
        arg_copy = NULL;
        argi++;
        if (argi >= argc) {
            break;
        }
        obj = json_object_array_get_idx(args, argi);
        arg = json_object_get_string(obj);
    }

    /* Output results */
    *ppath = path;
    *pargv = argv;
    argv = NULL;
    grc = TLOG_RC_OK;
cleanup:
    free(arg_copy);
    if (argv != NULL) {
        for (argi = 0; argv[argi] != NULL; argi++) {
            free(argv[argi]);
        }
        free(argv);
    }
    return grc;
}
