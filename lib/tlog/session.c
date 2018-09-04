/*
 * Handling of the process audit session.
 *
 * Copyright (C) 2015-2017 Red Hat
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
#include <tlog/session.h>
#include <tlog/misc.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

tlog_grc
tlog_session_get_id(unsigned int *pid)
{
    FILE *file;
    int orig_errno;
    int rc;

    assert(pid != NULL);

    file = fopen("/proc/self/sessionid", "r");
    if (file == NULL) {
        return TLOG_RC_FAILURE;
    }
    rc = fscanf(file, "%u", pid);
    orig_errno = errno;
    fclose(file);
    if (rc == 1) {
        return TLOG_RC_OK;
    }
    if (rc == 0) {
        return TLOG_GRC_FROM(errno, EINVAL);
    }

    return TLOG_GRC_FROM(errno, orig_errno);
}

/**
 * Format a session lock file path.
 *
 * @param ppath Location for the pointer to the dynamically-allocated
 *              formatted path. Not modified in case of error.
 * @param id    Session ID to format lock file path for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_session_format_lock_path(char **ppath, unsigned int id)
{
    tlog_grc grc;
    char *path;

    assert(ppath != NULL);

    if (asprintf(&path, TLOG_SESSION_LOCK_DIR "/session.%u.lock", id) < 0) {
        grc = TLOG_GRC_ERRNO;
        /* Silence Clang scanner warnings */
        if (grc == TLOG_RC_OK) {
            grc = TLOG_RC_FAILURE;
        }
        goto cleanup;
    }

    *ppath = path;
    grc = TLOG_RC_OK;
cleanup:
    return grc;
}

tlog_grc
tlog_session_lock(struct tlog_errs **perrs, unsigned int id,
                  uid_t euid, gid_t egid, bool *pacquired)
{
    /*
     * It's better to assume we locked the session,
     * as it is better to record a session than not.
     */
    bool acquired = true;
    tlog_grc grc;
    char *path = NULL;
    int fd = -1;

    /* Format lock file path */
    grc = tlog_session_format_lock_path(&path, id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed formatting lock file path");
    }

    /*
     * Attempt to create session lock file.
     * Assume session ID never repeats (never wraps around).
     * FIXME Handle repeating session IDs.
     */
    TLOG_EVAL_WITH_EUID_EGID(euid, egid,
                             fd = open(path, O_CREAT | O_EXCL,
                                       S_IRUSR | S_IWUSR));
    if (fd < 0) {
        int open_errno = errno;
        tlog_grc open_grc = TLOG_GRC_ERRNO;
        if (open_errno == EEXIST) {
            acquired = false;
        } else {
            tlog_errs_pushc(perrs, open_grc);
            tlog_errs_pushf(perrs, "Failed creating lock file %s", path);
            switch (open_errno) {
            case EPERM:
            case EACCES:
            case ENOENT:
            case ENOTDIR:
                tlog_errs_pushs(perrs, "Assuming session was unlocked");
                break;
            default:
                grc = open_grc;
                goto cleanup;
            }
        }
    }

    *pacquired = acquired;
    grc = TLOG_RC_OK;

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    free(path);
    return grc;
}

tlog_grc
tlog_session_unlock(struct tlog_errs **perrs, unsigned int id,
                    uid_t euid, gid_t egid)
{
    int rc;
    tlog_grc grc;
    char *path = NULL;

    /* Format lock file path */
    grc = tlog_session_format_lock_path(&path, id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed formatting lock file path");
    }

    /* Remove the lock file */
    TLOG_EVAL_WITH_EUID_EGID(euid, egid, rc = unlink(path));
    if (rc < 0) {
        int unlink_errno = errno;
        tlog_grc unlink_grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, unlink_grc);
        tlog_errs_pushf(perrs, "Failed removing lock file %s", path);
        switch (unlink_errno) {
        case ENOENT:
            tlog_errs_pushs(perrs, "Ignoring non-existent lock file");
            break;
        default:
            grc = unlink_grc;
            goto cleanup;
        }
    }

    grc = TLOG_RC_OK;

cleanup:
    free(path);
    return grc;
}
