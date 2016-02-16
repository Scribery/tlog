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
#include <assert.h>
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
#include <time.h>
#include <tlog/syslog_json_writer.h>
#include <tlog/tty_source.h>
#include <tlog/json_sink.h>
#include <tlog/tty_sink.h>
#include <tlog/rc.h>

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

/* Number of ALRM signals caught */
static volatile sig_atomic_t alarm_caught = 0;

static void
alarm_sighandler(int signum)
{
    (void)signum;
    alarm_caught++;
}

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
    struct tlog_json_writer *log_writer = NULL;
    const struct timespec delay_min = TLOG_DELAY_MIN_TIMESPEC;
    clockid_t clock_id;
    struct timespec timestamp;
    struct tlog_sink *log_sink = NULL;
    struct tlog_sink *tty_sink = NULL;
    struct tlog_source *tty_source = NULL;
    tlog_grc grc;
    ssize_t rc;
    int master_fd;
    pid_t child_pid;
    unsigned int session_id;
    sig_atomic_t last_alarm_caught = 0;
    sig_atomic_t new_alarm_caught;
    bool alarm_set = false;
    bool log_pending = false;
    bool term_attrs_set = false;
    struct termios orig_termios;
    struct termios raw_termios;
    struct winsize winsize;
    size_t i, j;
    struct sigaction sa;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    struct tlog_pkt_pos tty_pos = TLOG_PKT_POS_VOID;
    struct tlog_pkt_pos log_pos = TLOG_PKT_POS_VOID;
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
    grc = tlog_syslog_json_writer_create(&log_writer, LOG_INFO);
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
        tlog_timespec_cmp(&timestamp, &delay_min) <= 0) {
        clock_id = CLOCK_MONOTONIC_COARSE;
    } else if (clock_getres(CLOCK_MONOTONIC, NULL) == 0) {
        clock_id = CLOCK_MONOTONIC;
    } else {
        fprintf(stderr, "No clock to use\n");
        goto cleanup;
    }

    /* Create the log sink */
    grc = tlog_json_sink_create(&log_sink, log_writer, false,
                                fqdn, passwd->pw_name,
                                session_id, BUF_SIZE);
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
    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN)
        {
            sa.sa_handler = exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < TLOG_ARRAY_SIZE(exit_sig); j++)
                sigaddset(&sa.sa_mask, exit_sig[j]);
            /* NOTE: no SA_RESTART on purpose */
            sa.sa_flags = 0;
            sigaction(exit_sig[i], &sa, NULL);
        }
    }

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

    /* Create the TTY source */
    grc = tlog_tty_source_create(&tty_source, STDIN_FILENO,
                                 master_fd, STDOUT_FILENO,
                                 BUF_SIZE, clock_id);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating TTY source: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Create the TTY sink */
    grc = tlog_tty_sink_create(&tty_sink, master_fd,
                               STDOUT_FILENO, master_fd);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating TTY sink: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /*
     * Transfer I/O and window changes
     */
    while (exit_signum == 0) {
        /* Handle latency limit */
        new_alarm_caught = alarm_caught;
        if (new_alarm_caught != last_alarm_caught) {
            alarm_set = false;
            grc = tlog_sink_flush(log_sink);
            if (grc != TLOG_RC_OK) {
                fprintf(stderr, "Failed flushing log: %s\n",
                        tlog_grc_strerror(grc));
                goto cleanup;
            }
            last_alarm_caught = new_alarm_caught;
            log_pending = false;
        } else if (log_pending && !alarm_set) {
            alarm(IO_LATENCY);
            alarm_set = true;
        }

        /* Deliver logged data if any */
        if (tlog_pkt_pos_cmp(&tty_pos, &log_pos) < 0) {
            grc = tlog_sink_write(tty_sink, &pkt, &tty_pos, &log_pos);
            if (grc != TLOG_RC_OK) {
                if (grc == TLOG_GRC_FROM(errno, EINTR)) {
                    continue;
                } else if (grc == TLOG_GRC_FROM(errno, EBADF) ||
                           grc == TLOG_GRC_FROM(errno, EINVAL)) {
                    status = 0;
                } else {
                    fprintf(stderr, "Failed writing terminal data: %s\n",
                            tlog_grc_strerror(grc));
                }
                break;
            }
            continue;
        }

        /* Log the received data, if any */
        if (tlog_pkt_pos_is_in(&log_pos, &pkt)) {
            grc = tlog_sink_write(log_sink, &pkt, &log_pos, NULL);
            if (grc != TLOG_RC_OK &&
                grc != TLOG_GRC_FROM(errno, EINTR)) {
                fprintf(stderr, "Failed logging terminal data: %s\n",
                        tlog_grc_strerror(grc));
                break;
            }
            log_pending = true;
            continue;
        }

        /* Read new data */
        tlog_pkt_cleanup(&pkt);
        log_pos = TLOG_PKT_POS_VOID;
        tty_pos = TLOG_PKT_POS_VOID;
        grc = tlog_source_read(tty_source, &pkt);
        if (grc != TLOG_RC_OK) {
            if (grc == TLOG_GRC_FROM(errno, EINTR)) {
                continue;
            } else if (grc == TLOG_GRC_FROM(errno, EBADF) ||
                       grc == TLOG_GRC_FROM(errno, EIO)) {
                status = 0;
            } else {
                fprintf(stderr, "Failed reading terminal data: %s\n",
                        tlog_grc_strerror(grc));
            }
            break;
        } else if (tlog_pkt_is_void(&pkt)) {
            status = 0;
            break;
        }
    }

    /* Cut the log (write incomplete characters as binary) */
    grc = tlog_sink_cut(log_sink);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed cutting-off the log: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Flush the log */
    grc = tlog_sink_flush(log_sink);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed flushing the log: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

cleanup:

    tlog_sink_destroy(log_sink);
    tlog_json_writer_destroy(log_writer);
    tlog_sink_destroy(tty_sink);
    tlog_source_destroy(tty_source);
    free(fqdn);

    /* Restore signal handlers */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
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
