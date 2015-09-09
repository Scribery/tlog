/*
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

#include <pty.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pwd.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <stdbool.h>
#include <poll.h>
#include <stdio.h>
#include <syslog.h>
#include <tlog/syslog_writer.h>
#include <tlog/sink.h>

#define IO_LATENCY  3
#define BUF_SIZE    4096

/**< Number of the signal causing exit */
static volatile sig_atomic_t exit_signum  = 0;

static void
exit_sighandler(int signum)
{
    if (exit_signum == 0)
        exit_signum = signum;
}

/* Number of WINCH signals caught */
static volatile sig_atomic_t sigwinch_caught = 0;

static void
winch_sighandler(int signum)
{
    (void)signum;
    sigwinch_caught++;
}

/* Number of ALRM signals caught */
static volatile sig_atomic_t alarm_caught = 0;

static void
alarm_sighandler(int signum)
{
    (void)signum;
    alarm_caught++;
}

enum fd_idx {
    FD_IDX_STDIN,
    FD_IDX_MASTER,
    FD_IDX_NUM
};

/**
 * Get process session ID.
 *
 * @param pid   Location for the retrieved session ID.
 *
 * @return Global return code.
 */
static tlog_grc
get_session_id(unsigned int *pid)
{
    FILE *file;
    int orig_errno;
    int rc;

    assert(pid != NULL);

    file = fopen("/proc/self/sessionid", "r");
    if (file == NULL)
        return TLOG_RC_FAILURE;
    rc = fscanf(file, "%u", pid);
    orig_errno = errno;
    fclose(file);
    if (rc == 1)
        return TLOG_RC_OK;
    if (rc == 0)
        return TLOG_GRC_FROM(errno, EINVAL);

    return TLOG_GRC_FROM(errno, orig_errno);
}

/**
 * Get fully-qualified name of this host.
 *
 * @param pfqdn     Location for the dynamically-allocated name.
 *
 * @return Status code.
 */
static tlog_grc
get_fqdn(char **pfqdn)
{
    char hostname[HOST_NAME_MAX + 1];
    struct addrinfo hints = {.ai_family = AF_UNSPEC,
                             .ai_flags  = AI_CANONNAME};
    struct addrinfo *info;
    int gai_error;

    assert(pfqdn != NULL);

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) < 0)
        return TLOG_GRC_ERRNO;

    /* Resolve hostname to FQDN */
    gai_error = getaddrinfo(hostname, NULL, &hints, &info);
    if (gai_error != 0)
        return TLOG_GRC_FROM(gai, gai_error);

    /* Duplicate retrieved FQDN */
    *pfqdn = strdup(info->ai_canonname);
    freeaddrinfo(info);
    if (*pfqdn == NULL)
        return TLOG_GRC_ERRNO;

    return TLOG_RC_OK;
}

int
main(int argc, char **argv)
{
    const int exit_sig[] = {SIGINT, SIGTERM, SIGHUP};
    char *fqdn = NULL;
    struct passwd *passwd;
    struct tlog_writer *writer = NULL;
    clockid_t clock_id;
    struct timespec timestamp;
    struct tlog_sink *sink = NULL;
    tlog_grc grc;
    ssize_t rc;
    int master_fd;
    pid_t child_pid;
    unsigned int session_id;
    sig_atomic_t last_sigwinch_caught = 0;
    sig_atomic_t new_sigwinch_caught;
    sig_atomic_t last_alarm_caught = 0;
    sig_atomic_t new_alarm_caught;
    bool alarm_set = false;
    bool io_pending = false;
    bool term_attrs_set = false;
    struct termios orig_termios;
    struct termios raw_termios;
    struct winsize winsize;
    struct winsize new_winsize;
    size_t i, j;
    struct sigaction sa;
    struct pollfd fds[FD_IDX_NUM];
    uint8_t input_buf[BUF_SIZE];
    size_t input_pos = 0;
    size_t input_len = 0;
    uint8_t output_buf[BUF_SIZE];
    size_t output_pos = 0;
    size_t output_len = 0;
    int status = 1;

    (void)argv;
    if (argc > 1) {
        fprintf(stderr, "Arguments are not accepted\n");
        goto cleanup;
    }

    /* Get host FQDN */
    grc = get_fqdn(&fqdn);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed retrieving host FQDN: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Get session ID */
    grc = get_session_id(&session_id);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed retrieving session ID: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Open syslog */
    openlog("tlog", LOG_NDELAY, LOG_LOCAL0);
    /* Create the syslog writer */
    grc = tlog_syslog_writer_create(&writer, LOG_INFO);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating syslog writer: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Get effective user entry */
    errno = 0;
    passwd = getpwuid(geteuid());
    if (passwd == NULL) {
        if (errno == 0)
            fprintf(stderr, "User entry not found\n");
        else
            fprintf(stderr, "Failed retrieving user entry: %s\n",
                    strerror(errno));
        goto cleanup;
    }

    /*
     * Choose the clock: try to use coarse monotonic clock (which is faster),
     * if it provides resolution of at least one millisecond.
     */
    if (clock_getres(CLOCK_MONOTONIC_COARSE, &timestamp) == 0 &&
        timestamp.tv_sec == 0 && timestamp.tv_nsec < 1000000) {
        clock_id = CLOCK_MONOTONIC_COARSE;
    } else if (clock_getres(CLOCK_MONOTONIC, NULL) == 0) {
        clock_id = CLOCK_MONOTONIC;
    } else {
        fprintf(stderr, "No clock to use\n");
        goto cleanup;
    }

    /* Get startup timestamp */
    clock_gettime(clock_id, &timestamp);

    /* Create the log sink */
    grc = tlog_sink_create(&sink, writer, fqdn, passwd->pw_name,
                           session_id, BUF_SIZE, &timestamp);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating log sink: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Get terminal attributes */
    rc = tcgetattr(STDOUT_FILENO, &orig_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed retrieving tty attributes: %s\n",
                strerror(errno));
        goto cleanup;
    }

    /* Get terminal window size */
    rc = ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize);
    if (rc < 0) {
        fprintf(stderr, "Failed retrieving tty window size: %s\n",
                strerror(errno));
        goto cleanup;
    }

    /* Fork a child under a slave pty */
    child_pid = forkpty(&master_fd, NULL, &orig_termios, &winsize);
    if (child_pid < 0) {
        fprintf(stderr, "Failed forking a pty: %s\n", strerror(errno));
        goto cleanup;
    } else if (child_pid == 0) {
        /*
         * Child
         */
        execl("/bin/bash", "/bin/bash", NULL);
        fprintf(stderr, "Failed to execute the shell: %s",
                strerror(errno));
        goto cleanup;
    }

    /*
     * Parent
     */
    /* Log initial window size */
    grc = tlog_sink_window_write(sink, &timestamp,
                                 winsize.ws_col, winsize.ws_row);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed logging window size: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN)
        {
            sa.sa_handler = exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < ARRAY_SIZE(exit_sig); j++)
                sigaddset(&sa.sa_mask, exit_sig[j]);
            /* NOTE: no SA_RESTART on purpose */
            sa.sa_flags = 0;
            sigaction(exit_sig[i], &sa, NULL);
        }
    }

    /* Setup WINCH signal handler */
    sa.sa_handler = winch_sighandler;
    sigemptyset(&sa.sa_mask);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    /* Setup ALRM signal handler */
    sa.sa_handler = alarm_sighandler;
    sigemptyset(&sa.sa_mask);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    /* Switch the terminal to raw mode */
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    raw_termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                             INPCK | ISTRIP | IXON | PARMRK);
    raw_termios.c_oflag &= ~OPOST;
    raw_termios.c_cc[VMIN] = 1;
    raw_termios.c_cc[VTIME] = 0;
    rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed setting tty attributes: %s\n",
                strerror(errno));
        goto cleanup;
    }
    term_attrs_set = true;

    /*
     * Transfer I/O and window changes
     */
    fds[FD_IDX_STDIN].fd        = STDIN_FILENO;
    fds[FD_IDX_STDIN].events    = POLLIN;
    fds[FD_IDX_MASTER].fd       = master_fd;
    fds[FD_IDX_MASTER].events   = POLLIN;

    while (exit_signum == 0) {
        /*
         * Handle SIGWINCH
         */
        new_sigwinch_caught = sigwinch_caught;
        if (new_sigwinch_caught != last_sigwinch_caught) {
            /* Retrieve window size */
            rc = ioctl(STDOUT_FILENO, TIOCGWINSZ, &new_winsize);
            if (rc < 0) {
                if (errno == EBADF)
                    status = 0;
                else
                    fprintf(stderr, "Failed retrieving window size: %s\n",
                            strerror(errno));
                break;
            }
            /* If window size has changed */
            if (new_winsize.ws_row != winsize.ws_row ||
                new_winsize.ws_col != winsize.ws_col) {
                /* Log window size */
                clock_gettime(clock_id, &timestamp);
                grc = tlog_sink_window_write(sink, &timestamp,
                                             new_winsize.ws_col,
                                             new_winsize.ws_row);
                if (grc != TLOG_RC_OK) {
                    fprintf(stderr, "Failed logging window size: %s\n",
                            tlog_grc_strerror(grc));
                    break;
                }
                /* Propagate window size */
                rc = ioctl(master_fd, TIOCSWINSZ, &new_winsize);
                if (rc < 0) {
                    if (errno == EBADF)
                        status = 0;
                    else
                        fprintf(stderr, "Failed setting window size: %s\n",
                                strerror(errno));
                    break;
                }
                winsize = new_winsize;
            }
            /* Mark SIGWINCH processed */
            last_sigwinch_caught = new_sigwinch_caught;
        }

        /*
         * Deliver I/O
         */
        clock_gettime(clock_id, &timestamp);

        if (input_pos < input_len) {
            rc = write(master_fd, input_buf + input_pos,
                       input_len - input_pos);
            if (rc >= 0) {
                if (rc > 0) {
                    /* Log delivered input */
                    grc = tlog_sink_io_write(sink, &timestamp,
                                             false, input_buf + input_pos,
                                             (size_t)rc);
                    if (grc != TLOG_RC_OK) {
                        fprintf(stderr, "Failed logging input: %s\n",
                                tlog_grc_strerror(grc));
                        break;
                    }
                    io_pending = true;
                    input_pos += rc;
                }
                /* If interrupted by a signal handler */
                if (input_pos < input_len)
                    continue;
                /* Exhausted */
                input_pos = input_len = 0;
            } else {
                if (errno == EINTR)
                    continue;
                else if (errno == EBADF || errno == EINVAL)
                    status = 0;
                else
                    fprintf(stderr, "Failed to write to master: %s\n",
                            strerror(errno));
                break;
            };
        }
        if (output_pos < output_len) {
            rc = write(STDOUT_FILENO, output_buf + output_pos,
                       output_len - output_pos);
            if (rc >= 0) {
                if (rc > 0) {
                    /* Log delivered output */
                    grc = tlog_sink_io_write(sink, &timestamp,
                                             true, output_buf + output_pos,
                                             (size_t)rc);
                    if (grc != TLOG_RC_OK) {
                        fprintf(stderr, "Failed logging output: %s\n",
                                tlog_grc_strerror(grc));
                        break;
                    }
                    io_pending = true;
                    output_pos += rc;
                }
                /* If interrupted by a signal handler */
                if (output_pos < output_len)
                    continue;
                /* Exhausted */
                output_pos = output_len = 0;
            } else {
                if (errno == EINTR)
                    continue;
                else if (errno == EBADF || errno == EINVAL)
                    status = 0;
                else
                    fprintf(stderr, "Failed to write to stdout: %s\n",
                            strerror(errno));
                break;
            };
        }

        /*
         * Handle I/O latency limit
         */
        new_alarm_caught = alarm_caught;
        if (new_alarm_caught != last_alarm_caught) {
            alarm_set = false;
            grc = tlog_sink_io_flush(sink);
            if (grc != TLOG_RC_OK) {
                fprintf(stderr, "Failed flushing I/O log: %s\n",
                        tlog_grc_strerror(grc));
                goto cleanup;
            }
            last_alarm_caught = new_alarm_caught;
            io_pending = false;
        } else if (io_pending && !alarm_set) {
            alarm(IO_LATENCY);
            alarm_set = true;
        }

        /*
         * Wait for I/O
         */
        rc = poll(fds, sizeof(fds) / sizeof(fds[0]), -1);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            else
                fprintf(stderr, "Failed waiting for I/O: %s\n",
                        strerror(errno));
            break;
        }

        /*
         * Retrieve I/O
         *
         * NOTE: Reading master first in case child went away.
         *       Otherwise writing to it can block
         */
        if (fds[FD_IDX_MASTER].revents & (POLLIN | POLLHUP | POLLERR)) {
            rc = read(master_fd, output_buf, sizeof(output_buf));
            if (rc <= 0) {
                if (rc < 0 && errno == EINTR)
                    continue;
                status = 0;
                break;
            }
            output_len = rc;
        }
        if (fds[FD_IDX_STDIN].revents & (POLLIN | POLLHUP | POLLERR)) {
            rc = read(STDIN_FILENO, input_buf, sizeof(input_buf));
            if (rc <= 0) {
                if (rc < 0 && errno == EINTR)
                    continue;
                status = 0;
                break;
            }
            input_len = rc;
        }
    }

    /* Cut I/O log (write incomplete characters as binary) */
    grc = tlog_sink_io_cut(sink);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed cutting-off I/O log: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Flush I/O log */
    grc = tlog_sink_io_flush(sink);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed flushing I/O log: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

cleanup:

    tlog_sink_destroy(sink);
    tlog_writer_destroy(writer);
    free(fqdn);

    /* Restore signal handlers */
    for (i = 0; i < ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN)
            signal(exit_sig[i], SIG_DFL);
    }

    /* Restore terminal attributes */
    if (term_attrs_set) {
        rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &orig_termios);
        if (rc < 0 && errno != EBADF) {
            fprintf(stderr, "Failed restoring tty attributes: %s\n",
                    strerror(errno));
            return 1;
        }
    }

    /* Reproduce the exit signal to get proper exit status */
    if (exit_signum != 0)
        raise(exit_signum);

    return status;
}
