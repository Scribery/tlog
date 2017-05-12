/**
 * @file
 * @brief Handling of the process audit session.
 */
/*
 * Copyright (C) 2017 Red Hat
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

#ifndef _TLOG_SESSION_H
#define _TLOG_SESSION_H

#include <tlog/grc.h>
#include <tlog/errs.h>
#include <sys/types.h>

/**
 * Get process session ID.
 *
 * @param pid   Location for the retrieved session ID.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_session_get_id(unsigned int *pid);

/**
 * Lock a session.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param id        ID of the session to lock
 * @param euid      EUID to use while creating the lock.
 * @param egid      EGID to use while creating the lock.
 * @param pacquired Location for the flag which is set to true if the session
 *                  lock was acquired , and to false if the session was
 *                  already locked. Not modified in case of error.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_session_lock(struct tlog_errs **perrs, unsigned int id,
                                  uid_t euid, gid_t egid, bool *pacquired);

/**
 * Unlock a session.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param id        ID of the session to unlock
 * @param euid      EUID to use while removing the lock.
 * @param egid      EGID to use while removing the lock.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_session_unlock(struct tlog_errs **perrs, unsigned int id,
                                    uid_t euid, gid_t egid);

#endif /* _TLOG_SESSION_H */
