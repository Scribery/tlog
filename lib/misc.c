/*
 * Miscellaneous functions.
 *
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

#include "config.h"
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

const char *tlog_version =
    PACKAGE_STRING "\n"
    "Copyright (C) 2015-2016 Red Hat\n"
    "License GPLv2+: GNU GPL version 2 or later "
        "<http://gnu.org/licenses/gpl.html>.\n"
    "\n"
    "This is free software: "
        "you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n";

tlog_grc
tlog_build_or_inst_path(char          **ppath,
                        const char     *prog_path,
                        const char     *build_rel_path,
                        const char     *inst_abs_path)
{
    tlog_grc grc = TLOG_RC_FAILURE;
    char *abs_prog_path = NULL;
    const char *prog_name;
    const char *prog_dir;
    char *rel_path = NULL;
    char *abs_path = NULL;

    assert(ppath != NULL);
    assert(prog_path != NULL);
    assert(inst_abs_path != NULL);
    assert(build_rel_path != NULL);

    /* If we can get the program's path and thus attach the relative path */
    abs_prog_path = realpath(prog_path, NULL);
    if (abs_prog_path != NULL) {
        /* Assume we get the GNU version, which doesn't modify the buffer */
        prog_name = basename(abs_prog_path);

        /* Skip the initial dash - login shell flag, if any */
        if (prog_name[0] == '-') {
            prog_name++;
        }

        /* If running from the build dir (have "lt-" prefix) */
        if (prog_name[0] == 'l' && prog_name[1] == 't' && prog_name[2] == '-') {
            /* Form absolute path from relative */
            prog_dir = dirname(abs_prog_path);
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

    *ppath = abs_path;
    abs_path = NULL;
    grc = TLOG_RC_OK;
cleanup:
    free(abs_path);
    free(rel_path);
    free(abs_prog_path);
    return grc;
}
