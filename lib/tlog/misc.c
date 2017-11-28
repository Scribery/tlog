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

#include <config.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <unistd.h>
#include <sys/types.h>
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
    char *prog_dir = NULL;
    char *prog_dir_copy = NULL;
    const char *prog_dir_name;
    char *rel_path = NULL;
    char *abs_path = NULL;

    assert(ppath != NULL);
    assert(prog_path != NULL);
    assert(inst_abs_path != NULL);
    assert(build_rel_path != NULL);

    /* If we can get the program's path and thus attach the relative path */
    abs_prog_path = realpath(prog_path, NULL);
    if (abs_prog_path != NULL) {
        /*
         * Check if the last path component is the libtool's ".libs" dir
         * created in the build tree.
         */
        /* Get the executable's directory */
        prog_dir = strdup(dirname(abs_prog_path));
        if (prog_dir == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto cleanup;
        }
        /* Get the executable's directory name */
        prog_dir_copy = strdup(prog_dir);
        if (prog_dir_copy == NULL) {
            grc = TLOG_GRC_ERRNO;
            goto cleanup;
        }
        prog_dir_name = basename(prog_dir_copy);

        /* If running from the build dir (executable under ".libs") */
        if (strcmp(prog_dir_name, ".libs") == 0) {
            /* Form absolute path from relative */
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
    } else if (errno != ENOENT && errno != EACCES && errno != ENOTDIR) {
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
    free(prog_dir_copy);
    free(prog_dir);
    free(abs_prog_path);
    return grc;
}

tlog_grc
tlog_exec(struct tlog_errs **perrs, unsigned int opts,
          const char *path, char **argv)
{
    tlog_grc grc;

    /* If requested to drop privileges */
    if (opts & TLOG_EXEC_OPT_DROP_PRIVS) {
        uid_t uid = getuid();
        gid_t gid = getgid();

        /* Drop the possibly-privileged EGID permanently */
        if (setresgid(gid, gid, gid) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed dropping EGID");
            goto cleanup;
        }

        /* Drop the possibly-privileged EUID permanently */
        if (setresuid(uid, uid, uid) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed dropping EUID");
            goto cleanup;
        }
    }

    /* Exec the program */
    if (opts & TLOG_EXEC_OPT_SEARCH_PATH) {
        execvp(path, argv);
    } else {
        execv(path, argv);
    }
    grc = TLOG_GRC_ERRNO;
    tlog_errs_pushc(perrs, grc);
    tlog_errs_pushf(perrs, "Failed executing %s", path);

cleanup:
    return grc;
}
