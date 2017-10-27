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
               clockid_t clock_id)
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
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving TTY attributes");
        goto cleanup;
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
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed mmapping privilege-locking semaphore");
        goto cleanup;
    }
    /* Initialize privilege-locking semaphore */
    if (sem_init(sem, 1, 0) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating privilege-locking semaphore");
        goto cleanup;
    }
    sem_initialized = true;

    /* If we've got a TTY */
    if (tap.tty_fd >= 0) {
        struct winsize winsize;
        int master_fd;

        /* Get terminal window size */
        if (ioctl(tap.tty_fd, TIOCGWINSZ, &winsize) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed retrieving TTY window size");
            goto cleanup;
        }

        /* Fork a child connected via a PTY */
        tap.pid = forkpty(&master_fd, NULL, &tap.termios_orig, &winsize);
        if (tap.pid < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed forking a child connected via a PTY");
            goto cleanup;
        }
        /* If parent */
        if (tap.pid != 0) {
            /* Split master FD into input and output FDs */
            tap.in_fd = master_fd;
            tap.out_fd = dup(master_fd);
            if (tap.out_fd < 0) {
                grc = TLOG_GRC_ERRNO;
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed duplicating PTY master FD");
                goto cleanup;
            }
        }
    } else {
        /* Fork a child connected via pipes */
        tap.pid = tlog_tap_forkpipes(in_fd >= 0 ? &tap.in_fd : NULL,
                                     out_fd >= 0 ? &tap.out_fd : NULL);
        if (tap.pid < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs,
                            "Failed forking a child connected via pipes");
            goto cleanup;
        }
    }

    /* Execute the program to record in the child */
    if (tap.pid == 0) {
        /* Wait for the parent to lock the privileges */
        if (sem_wait(sem) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs,
                            "Failed waiting for parent to lock privileges");
            goto cleanup;
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
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushf(perrs, "Failed locking GID");
        goto cleanup;
    }

    /* Lock the possibly-privileged UID permanently */
    if (setresuid(euid, euid, euid) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushf(perrs, "Failed locking UID");
        goto cleanup;
    }

    /* Signal the child we locked our privileges */
    if (sem_post(sem) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushf(perrs, "Failed signaling the child "
                               "the privileges are locked");
        goto cleanup;
    }

    /* Create the TTY source */
    grc = tlog_tty_source_create(&tap.source, in_fd, tap.out_fd,
                                 tap.tty_fd, 4096, clock_id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating TTY source");
        goto cleanup;
    }

    /* Create the TTY sink */
    grc = tlog_tty_sink_create(&tap.sink, tap.in_fd, out_fd,
                               tap.tty_fd >= 0 ? tap.in_fd : -1);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating TTY sink");
        goto cleanup;
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
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed setting TTY attributes");
            goto cleanup;
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
    tlog_grc grc;

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
            return grc;
        }
        tap->termios_set = false;

        /* Clear off remaining child output */
        rc = write(tap->tty_fd, &newline, sizeof(newline));
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed writing newline to TTY");
            return grc;
        }
    }

    /* Wait for the child, if any */
    if (tap->pid > 0) {
        if (waitpid(tap->pid, pstatus, 0) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed waiting for the child");
            return grc;
        }
        tap->pid = 0;
    } else {
        if (pstatus != NULL) {
            *pstatus = 0;
        }
    }

    return TLOG_RC_OK;
}
