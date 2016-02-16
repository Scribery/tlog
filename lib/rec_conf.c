/*
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
#include <tlog/rec_conf_origin.h>
#include <tlog/rec_conf_cmd.h>
#include <tlog/rec_conf_validate.h>
#include <tlog/json_misc.h>
#include <tlog/rc.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

static tlog_grc
tlog_rec_conf_str_parse(struct json_object **pconf,
                        const char *str,
                        enum tlog_rec_conf_origin origin)
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
        goto cleanup;
    }

    grc = tlog_rec_conf_validate(conf, origin);
    if (grc != TLOG_RC_OK) {
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
tlog_rec_conf_file_load(struct json_object **pconf,
                        const char *path)
{
    tlog_grc grc;
    int fd = -1;
    size_t size = 0;
    size_t len = 0;
    char *buf = NULL;
    char *new_buf;
    size_t new_size;
    ssize_t rc;

    assert(pconf != NULL);
    assert(path != NULL);

    /* Open the file */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed opening \"%s\": %s\n",
                path, tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Allocate the buffer */
    size = 2048;
    buf = malloc(size);
    if (buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed allocating reading buffer: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Read the whole file */
    while (true) {
        rc = read(fd, buf + len, size - len - 1);
        if (rc < 0) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed reading \"%s\": %s\n",
                    path, tlog_grc_strerror(grc));
            goto cleanup;
        } else if (rc == 0) {
            break;
        }

        len += rc;

        if (len > size / 2) {
            new_size = size * 2;
            new_buf = realloc(buf, new_size);
            if (new_buf == NULL) {
                grc = TLOG_GRC_ERRNO;
                fprintf(stderr, "Failed reallocating reading buffer: %s\n",
                        tlog_grc_strerror(grc));
                goto cleanup;
            }
            buf = new_buf;
            size = new_size;
        }
    }
    buf[len] = '\0';

    /* Parse the contents */
    grc = tlog_rec_conf_str_parse(pconf, buf, TLOG_REC_CONF_ORIGIN_FILE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed parsing \"%s\": %s\n",
                path, tlog_grc_strerror(grc));
        goto cleanup;
    }

    grc = TLOG_RC_OK;
cleanup:
    free(buf);
    if (fd >= 0) {
        close(fd);
    }
    return grc;
}

static tlog_grc
tlog_rec_conf_env_load(struct json_object **pconf)
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
        fprintf(stderr, "Failed creating configuration object: %s",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Load the config file, if specified */
    val = getenv("TLOG_REC_CONF_FILE");
    if (val != NULL) {
        grc = tlog_rec_conf_file_load(&overlay, val);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr,
                    "Failed loading the file referenced by "
                    "TLOG_REC_CONF_FILE environment variable: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        grc = tlog_json_overlay(&conf, conf, overlay);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
        json_object_put(overlay);
        overlay = NULL;
    }

    /* Load the config text, if specified */
    val = getenv("TLOG_REC_CONF_TEXT");
    if (val != NULL) {
        grc = tlog_rec_conf_str_parse(&overlay, val, TLOG_REC_CONF_ORIGIN_ENV);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr,
                    "Failed parsing the contents of "
                    "TLOG_REC_CONF_TEXT environment variable: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        grc = tlog_json_overlay(&conf, conf, overlay);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
        json_object_put(overlay);
        overlay = NULL;
    }

    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;
cleanup:
    json_object_put(overlay);
    json_object_put(conf);
    return grc;
}

static tlog_grc
tlog_rec_conf_get_path(char       **pabs_path,
                       const char  *prog_path,
                       const char  *inst_abs_path,
                       const char  *build_rel_path)
{
    tlog_grc grc;
    char *real_prog_path = NULL;
    const char *prog_name;
    const char *prog_dir;
    char *rel_path = NULL;
    char *abs_path = NULL;

    assert(pabs_path != NULL);
    assert(prog_path != NULL);
    assert(inst_abs_path != NULL);
    assert(build_rel_path != NULL);

    /* If we can get the program's path and thus attach the relative path */
    real_prog_path = realpath(prog_path, NULL);
    if (real_prog_path != NULL) {
        /* Assume we get the GNU version, which doesn't modify the buffer */
        prog_name = basename(real_prog_path);

        /* Skip the initial dash, if any */
        if (prog_name[0] == '-') {
            prog_name++;
        }

        /* If running from the build dir (have "lt-" prefix) */
        if (prog_name[0] == 'l' && prog_name[1] == 't' && prog_name[2] == '-') {
            /* Form absolute path from relative */
            prog_dir = dirname(real_prog_path);
            rel_path = malloc(strlen(prog_dir) + 1 + strlen(build_rel_path) + 1);
            if (rel_path == NULL) {
                grc = TLOG_GRC_ERRNO;
                goto cleanup;
            }
            strcpy(rel_path, prog_dir);
            strcat(rel_path, "/");
            strcat(rel_path, build_rel_path);
            abs_path = realpath(rel_path, NULL);
            if (abs_path == NULL) {
                grc = TLOG_GRC_ERRNO;
                goto cleanup;
            }
        }
    } else if (errno != ENOENT) {
        grc = TLOG_GRC_ERRNO;
        goto cleanup;
    }

    /* If the path wasn't found */
    if (abs_path == NULL) {
        /* Duplicate the specified absolute path */
        abs_path = strdup(inst_abs_path);
        if (abs_path == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto cleanup;
        }
    }

    *pabs_path = abs_path;
    abs_path = NULL;
    grc = TLOG_RC_OK;
cleanup:
    free(abs_path);
    free(rel_path);
    free(real_prog_path);
    return grc;
}

tlog_grc
tlog_rec_conf_load(struct json_object **pconf,
                   int argc, char **argv)
{
    tlog_grc grc;
    struct json_object *conf = NULL;
    struct json_object *overlay = NULL;
    char *path = NULL;

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
    GUARD(tlog_rec_conf_get_path(&path, argv[0], 
                                 TLOG_REC_CONF_DEFAULT_PATH,
                                 "../../lib/rec.conf"));
    GUARD(tlog_rec_conf_file_load(&overlay, path));
    free(path);
    path = NULL;
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with local system config */
    GUARD(tlog_rec_conf_get_path(&path, argv[0], 
                                 TLOG_REC_CONF_LOCAL_PATH,
                                 "../../lib/rec.conf"));
    GUARD(tlog_rec_conf_file_load(&overlay, path));
    free(path);
    path = NULL;
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with environment config */
    GUARD(tlog_rec_conf_env_load(&overlay));
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

    /* Overlay with command-line config */
    GUARD(tlog_rec_conf_cmd_load(&overlay, argc, argv));
    GUARD(tlog_json_overlay(&conf, conf, overlay));
    json_object_put(overlay);
    overlay = NULL;

#undef GUARD

    grc = TLOG_RC_OK;
    *pconf = conf;
    conf = NULL;
cleanup:
    free(path);
    json_object_put(overlay);
    json_object_put(conf);
    return grc;
}
