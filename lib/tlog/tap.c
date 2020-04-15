/*
 * I/O tap handling
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
#include <tlog/tap.h>
#include <tlog/tty_source.h>
#include <tlog/tty_sink.h>
#include <tlog/misc.h>
#include <pty.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>
#include <assert.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>

/**
 * Add an utmp entry for this session.
 *
 * @param tap   tlog_tap structure address.
 * @param path  The filename of the slave pty.
 * @param perrs Location for the error stack
 * @param euid  The EUID to use while updating utmp
 * @param egid  The EGID to use while updating utmp
 *
 * @return Global return code.
 */
static tlog_grc
tlog_tap_add_utmp_entry(struct tlog_tap *tap, char *path,
                        struct tlog_errs **perrs,
                        uid_t euid, gid_t egid)
{
    tlog_grc grc;
    int ofs;
    struct timeval tv;
    char *ses_line;
    struct utmpx ut;
    struct utmpx *ut_ptr;

    if ((ses_line = ttyname(tap->tty_fd)) == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed retrieving terminal name");
    }

    if (gettimeofday(&tv, NULL) < 0) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Failure obtaining the current time");
    }

    if ((tap->ut = calloc(sizeof(*tap->ut), 1)) == NULL) {
        grc = TLOG_GRC_FROM(errno, ENOMEM);
        goto cleanup;
    }

    /* Set basic utmp entry data */
    memset(&ut, 0, sizeof(struct utmpx));
    strncpy(ut.ut_user, getpwuid(getuid())->pw_name, sizeof(ut.ut_user));
    ut.ut_type = USER_PROCESS;
    ut.ut_pid = tap->pid;

    /* Strip the leading "/dev/" to get the line name */
    ofs = strncmp(path, TLOG_TAP_DEVPATH, TLOG_TAP_DEVSIZE) == 0 ? TLOG_TAP_DEVSIZE : 0;
    strcpy(ut.ut_line, path + ofs);

    /* Copy the trailing pty number to ut_id */
    strcpy(ut.ut_id, path + strlen(TLOG_TAP_PTSPATH));

    /* Set the session start time */
    ut.ut_tv.tv_sec = tv.tv_sec;
    ut.ut_tv.tv_usec = tv.tv_usec;

    /* Find the current session utmp entry, if any, and copy the remaining
     * data to the new entry */
    ofs = strncmp(ses_line, TLOG_TAP_DEVPATH, TLOG_TAP_DEVSIZE) == 0 ? TLOG_TAP_DEVSIZE : 0;
    ses_line += ofs;
    while ((ut_ptr = getutxent()) != NULL &&
            (ut_ptr->ut_type != USER_PROCESS || strncmp(ut_ptr->ut_line, ses_line, TLOG_TAP_UTMP_LINESIZE)))
        ;
    if (ut_ptr) {
        memcpy(ut.ut_host, ut_ptr->ut_host, sizeof(ut_ptr->ut_host));
        ut.ut_session = ut_ptr->ut_session;
        memcpy(ut.ut_addr_v6, ut_ptr->ut_addr_v6, sizeof(ut_ptr->ut_addr_v6));
    }

    /* Add the new session entry */
    setutxent();
    TLOG_EVAL_WITH_EUID_EGID(euid, egid, ut_ptr = pututxline(&ut));
    endutxent();

    if (ut_ptr == NULL) {
        free(tap->ut);
        tap->ut = NULL;
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed writing utmp entry to file");
    }

    memcpy(tap->ut, &ut, sizeof(ut));
    grc = TLOG_RC_OK;

cleanup:

    return grc;
}

/**
 * Remove the utmp entry for this session.
 *
 * @param tap       tlog_tap structure address.
 * @param wstatus   process status information.
 *
 * @return zero, or -1 in case of error with errno set.
 */
static int
tlog_tap_remove_utmp_entry(struct tlog_tap *tap, int wstatus)
{
    int res = 0;
    struct utmpx ut;

    if (tap->ut) {
        memcpy(&ut, tap->ut, sizeof(ut));
        free(tap->ut);
        tap->ut = NULL;
        ut.ut_type = DEAD_PROCESS;
        memset(&ut.ut_user, 0, sizeof(ut.ut_user));
        memset(ut.ut_host, 0, sizeof(ut.ut_host));
        ut.ut_exit.e_termination = WIFSIGNALED(wstatus) ? WTERMSIG(wstatus) : 0;
        ut.ut_exit.e_exit = WEXITSTATUS(wstatus);
        ut.ut_tv.tv_sec = 0;
        ut.ut_tv.tv_usec = 0;
        setutxent();
        if (pututxline(&ut) == NULL) {
            res = -1;
        }
        endutxent();
    }
    return res;
}

/**
 * Fork a child connected via a pair of pipes.
 *
 * @param pin_fd    Location for the FD of the input-writing pipe end, or NULL
 *                  if it should be closed.
 * @param pout_fd   Location for the FD of the output- and error-reading pipe
 *                  end, or NULL, if both should be closed.
 *
 * @return Child PID, or -1 in case of error with errno set.
 */
static int
tlog_tap_forkpipes(int *pin_fd, int *pout_fd)
{
    bool success = false;
    pid_t pid;
    int orig_errno;
    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};

#define GUARD(_expr) \
    do {                    \
        if ((_expr) < 0) {  \
            goto cleanup;   \
        }                   \
    } while (0)

    if (pin_fd != NULL) {
        GUARD(pipe(in_pipe));
    }
    if (pout_fd != NULL) {
        GUARD(pipe(out_pipe));
    }
    GUARD(pid = fork());

    if (pid == 0) {
        /*
         * Child
         */
        /* Close parent's pipe ends */
        if (pin_fd != NULL) {
            GUARD(close(in_pipe[1]));
            in_pipe[1] = -1;
        }
        if (pout_fd != NULL) {
            GUARD(close(out_pipe[0]));
            out_pipe[0] = -1;
        }

        /* Use child's pipe ends */
        if (pin_fd != NULL) {
            GUARD(dup2(in_pipe[0], STDIN_FILENO));
            GUARD(close(in_pipe[0]));
            in_pipe[0] = -1;
        } else {
            close(STDIN_FILENO);
        }
        if (pout_fd != NULL) {
            GUARD(dup2(out_pipe[1], STDOUT_FILENO));
            GUARD(dup2(out_pipe[1], STDERR_FILENO));
            GUARD(close(out_pipe[1]));
            out_pipe[1] = -1;
        } else {
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }
    } else {
        /*
         * Parent
         */
        /* Close child's pipe ends and output parent's pipe ends */
        if (pin_fd != NULL) {
            GUARD(close(in_pipe[0]));
            in_pipe[0] = -1;
            *pin_fd = in_pipe[1];
            in_pipe[1] = -1;
        }
        if (pout_fd != NULL) {
            GUARD(close(out_pipe[1]));
            out_pipe[1] = -1;
            *pout_fd = out_pipe[0];
            out_pipe[0] = -1;
        }
    }

#undef GUARD

    success = true;

cleanup:

    orig_errno = errno;
    if (in_pipe[0] >= 0) {
        close(in_pipe[0]);
    }
    if (in_pipe[1] >= 0) {
        close(in_pipe[1]);
    }
    if (out_pipe[0] >= 0) {
        close(out_pipe[0]);
    }
    if (out_pipe[1] >= 0) {
        close(out_pipe[1]);
    }
    errno = orig_errno;
    return success ? pid : -1;
}

tlog_grc
tlog_tap_setup(struct tlog_errs **perrs,
               struct tlog_tap *ptap,
               uid_t euid, gid_t egid,
               unsigned int opts, const char *path, char **argv,
               int in_fd, int out_fd, int err_fd,
               clockid_t clock_id,
               bool update_utmp)
{
    tlog_grc grc;
    struct tlog_tap tap = TLOG_TAP_VOID;
    sem_t *sem = MAP_FAILED;
    bool sem_initialized = false;

    assert(ptap != NULL);

    /* Try to find a TTY FD and get terminal attributes */
    errno = 0;
    if (in_fd >= 0 && tcgetattr(in_fd, &tap.termios_orig) >= 0) {
        tap.tty_fd = in_fd;
    } else if (errno == 0 || errno == ENOTTY || errno == EINVAL) {
        if (out_fd >= 0 && tcgetattr(out_fd, &tap.termios_orig) >= 0) {
            tap.tty_fd = out_fd;
        } else if (errno == 0 || errno == ENOTTY || errno == EINVAL) {
            if (err_fd >= 0 && tcgetattr(err_fd, &tap.termios_orig) >= 0) {
                tap.tty_fd = err_fd;
            }
        }
    }
    if (errno != 0 && errno != ENOTTY && errno != EINVAL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed retrieving TTY attributes");
    }

    /* Don't spawn a PTY if either input or output are closed */
    if (in_fd < 0 || out_fd < 0) {
        tap.tty_fd = -1;
    }

    /* Allocate privilege-locking semaphore */
    sem = mmap(NULL, sizeof(*sem), PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    if (sem == MAP_FAILED) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc,
                          "Failed mmapping privilege-locking semaphore");
    }
    /* Initialize privilege-locking semaphore */
    if (sem_init(sem, 1, 0) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc,
                          "Failed creating privilege-locking semaphore");
    }
    sem_initialized = true;

    /* If we've got a TTY */
    if (tap.tty_fd >= 0) {
        struct winsize winsize;
        int master_fd;
        char slave_path[PATH_MAX];

        /* Get terminal window size */
        if (ioctl(tap.tty_fd, TIOCGWINSZ, &winsize) < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed retrieving TTY window size");
        }

        /* Fork a child connected via a PTY */
        tap.pid = forkpty(&master_fd, slave_path, &tap.termios_orig, &winsize);
        if (tap.pid < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc,
                              "Failed forking a child connected via a PTY");
        }
        /* If parent */
        if (tap.pid != 0) {
            /* Split master FD into input and output FDs */
            tap.in_fd = master_fd;
            tap.out_fd = dup(master_fd);
            if (tap.out_fd < 0) {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc, "Failed duplicating PTY master FD");
            }

            /* Add the utmp entry */
            if (update_utmp) {
                grc = tlog_tap_add_utmp_entry(&tap, slave_path, perrs,
                                              euid, egid);
                if (grc != TLOG_RC_OK) {
                    TLOG_ERRS_RAISES("Failed adding utmp entry");
                }
            }
        }
    } else {
        /* Fork a child connected via pipes */
        tap.pid = tlog_tap_forkpipes(in_fd >= 0 ? &tap.in_fd : NULL,
                                     out_fd >= 0 ? &tap.out_fd : NULL);
        if (tap.pid < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc,
                              "Failed forking a child connected via pipes");
        }
    }

    /* Execute the program to record in the child */
    if (tap.pid == 0) {
        /* Wait for the parent to lock the privileges */
        if (sem_wait(sem) < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECF(grc,"Failed waiting for "
                              "parent to lock privileges");
        }

        /* Execute the program to record */
        tlog_exec(perrs, opts, path, argv);
        tlog_errs_print(stderr, *perrs);
        tlog_errs_destroy(perrs);
        _exit(127);
    }

    /* Lock the possibly-privileged GID permanently */
    if (setresgid(egid, egid, egid) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed locking GID");
    }

    /* Lock the possibly-privileged UID permanently */
    if (setresuid(euid, euid, euid) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed locking UID");
    }

    /* Signal the child we locked our privileges */
    if (sem_post(sem) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed signaling the child "
                          "the privileges are locked");
    }

    /* Create the TTY source */
    grc = tlog_tty_source_create(&tap.source, in_fd, tap.out_fd,
                                 tap.tty_fd, 4096, clock_id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating TTY source");
    }

    /* Create the TTY sink */
    grc = tlog_tty_sink_create(&tap.sink, tap.in_fd, out_fd,
                               tap.tty_fd >= 0 ? tap.in_fd : -1);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating TTY sink");
    }

    /* Switch the terminal to raw mode, if any */
    if (tap.tty_fd >= 0) {
        int rc;
        struct termios termios_raw = tap.termios_orig;
        termios_raw.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
        termios_raw.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                                 INPCK | ISTRIP | IXON | PARMRK);
        termios_raw.c_oflag &= ~OPOST;
        termios_raw.c_cc[VMIN] = 1;
        termios_raw.c_cc[VTIME] = 0;
        rc = tcsetattr(tap.tty_fd, TCSAFLUSH, &termios_raw);
        if (rc < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed setting TTY attributes");
        }
        tap.termios_set = true;
    }

    *ptap = tap;
    tap = TLOG_TAP_VOID;

cleanup:

    if (sem_initialized) {
        sem_destroy(sem);
    }
    if (sem != MAP_FAILED) {
        munmap(sem, sizeof(*sem));
    }
    tlog_tap_teardown(perrs, &tap, NULL);

    return grc;
}

tlog_grc
tlog_tap_teardown(struct tlog_errs **perrs,
                  struct tlog_tap *tap,
                  int *pstatus)
{
    tlog_grc grc = TLOG_RC_OK;
    int wstatus = 0;

    assert(tap != NULL);

    tlog_sink_destroy(tap->sink);
    tap->sink = NULL;
    tlog_source_destroy(tap->source);
    tap->source = NULL;

    if (tap->in_fd >= 0) {
        close(tap->in_fd);
        tap->in_fd = -1;
    }
    if (tap->out_fd >= 0) {
        close(tap->out_fd);
        tap->out_fd = -1;
    }

    /* Restore terminal attributes */
    if (tap->termios_set) {
        int rc;
        const char newline = '\n';

        rc = tcsetattr(tap->tty_fd, TCSAFLUSH, &tap->termios_orig);
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed restoring TTY attributes");
            goto cleanup;
        }
        tap->termios_set = false;

        /* Clear off remaining child output */
        rc = write(tap->tty_fd, &newline, sizeof(newline));
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed writing newline to TTY");
            goto cleanup;
        }
    }

    /* Wait for the child, if any */
    if (tap->pid > 0) {
        if (waitpid(tap->pid, &wstatus, 0) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed waiting for the child");
            goto cleanup;
        }
        tap->pid = 0;
        if (tlog_tap_remove_utmp_entry(tap, wstatus) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed removing utmp entry");
            goto cleanup;
        }
    }

cleanup:

    if (pstatus != NULL) {
        *pstatus = wstatus;
    }
    return grc;
}
