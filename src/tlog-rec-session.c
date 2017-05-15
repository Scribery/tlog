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

#include <config.h>
#include <pty.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <stdbool.h>
#include <poll.h>
#include <libgen.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <tlog/syslog_json_writer.h>
#include <tlog/fd_json_writer.h>
#include <tlog/json_sink.h>
#include <tlog/source.h>
#include <tlog/syslog_misc.h>
#include <tlog/timespec.h>
#include <tlog/delay.h>
#include <tlog/rc.h>
#include <tlog/session.h>
#include <tlog/tap.h>
#include <tlog/rec_session_conf.h>
#include <tlog/rec_session_conf_cmd.h>

/**
 * Let GNU TLS know we don't need implicit global initialization, which opens
 * /dev/urandom and can replace standard FDs in case they were closed.
 */
int
_gnutls_global_init_skip(void)
{
    return 1;
}

/**< Number of the signal causing exit */
static volatile sig_atomic_t exit_signum  = 0;

static void
exit_sighandler(int signum)
{
    if (exit_signum == 0) {
        exit_signum = signum;
    }
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
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        return TLOG_GRC_ERRNO;
    }

    /* Resolve hostname to FQDN */
    gai_error = getaddrinfo(hostname, NULL, &hints, &info);
    if (gai_error != 0) {
        return TLOG_GRC_FROM(gai, gai_error);
    }

    /* Duplicate retrieved FQDN */
    *pfqdn = strdup(info->ai_canonname);
    freeaddrinfo(info);
    if (*pfqdn == NULL) {
        return TLOG_GRC_ERRNO;
    }

    return TLOG_RC_OK;
}

/**
 * Create the log sink according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param psink         Location for the created sink pointer.
 * @param conf          Configuration JSON object.
 * @param session_id    The ID of the session being recorded.
 *
 * @return Global return code.
 */
static tlog_grc
create_log_sink(struct tlog_errs **perrs,
                struct tlog_sink **psink,
                struct json_object *conf,
                unsigned int session_id)
{
    tlog_grc grc;
    int64_t num;
    const char *str;
    struct json_object *obj;
    struct tlog_sink *sink = NULL;
    struct tlog_json_writer *writer = NULL;
    int fd = -1;
    int rc;
    char *fqdn = NULL;
    struct passwd *passwd;
    const char *term;

    /*
     * Create the writer
     */
    if (!json_object_object_get_ex(conf, "writer", &obj)) {
        tlog_errs_pushs(perrs, "Writer type is not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    str = json_object_get_string(obj);
    if (strcmp(str, "file") == 0) {
        struct json_object *conf_file;

        /* Get file writer conf container */
        if (!json_object_object_get_ex(conf, "file", &conf_file)) {
            tlog_errs_pushs(perrs, "File writer parameters are not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get the file path */
        if (!json_object_object_get_ex(conf_file, "path", &obj)) {
            tlog_errs_pushs(perrs, "Log file path is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);

        /* Open the file */
        fd = open(str, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed opening log file \"%s\"", str);
            goto cleanup;
        }
        /* Get file flags */
        rc = fcntl(fd, F_GETFD);
        if (rc < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed getting log file descriptor flags");
            goto cleanup;
        }
        /* Add FD_CLOEXEC to file flags */
        rc = fcntl(fd, F_SETFD, rc | FD_CLOEXEC);
        if (rc < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed setting log file descriptor flags");
            goto cleanup;
        }

        /* Create the writer, letting it take over the FD */
        grc = tlog_fd_json_writer_create(&writer, fd, true);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating file writer");
            goto cleanup;
        }
        fd = -1;
    } else if (strcmp(str, "syslog") == 0) {
        struct json_object *conf_syslog;
        int facility;
        int priority;

        /* Get syslog writer conf container */
        if (!json_object_object_get_ex(conf, "syslog", &conf_syslog)) {
            tlog_errs_pushs(perrs, "Syslog writer parameters are not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get facility */
        if (!json_object_object_get_ex(conf_syslog, "facility", &obj)) {
            tlog_errs_pushs(perrs, "Syslog facility is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);
        facility = tlog_syslog_facility_from_str(str);
        if (facility < 0) {
            tlog_errs_pushf(perrs, "Unknown syslog facility: %s", str);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get priority */
        if (!json_object_object_get_ex(conf_syslog, "priority", &obj)) {
            tlog_errs_pushs(perrs, "Syslog priority is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);
        priority = tlog_syslog_priority_from_str(str);
        if (priority < 0) {
            tlog_errs_pushf(perrs, "Unknown syslog priority: %s", str);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Create the writer */
        openlog("tlog", LOG_NDELAY, facility);
        grc = tlog_syslog_json_writer_create(&writer, priority);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating syslog writer");
            goto cleanup;
        }
    } else {
        tlog_errs_pushf(perrs, "Unknown writer type: %s", str);
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }

    /*
     * Create the sink
     */
    /* Get host FQDN */
    grc = get_fqdn(&fqdn);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving host FQDN");
        goto cleanup;
    }
    if (!tlog_utf8_str_is_valid(fqdn)) {
        tlog_errs_pushf(perrs, "Host FQDN is not valid UTF-8: %s", fqdn);
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }

    /* Get real user entry */
    errno = 0;
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
    if (!tlog_utf8_str_is_valid(passwd->pw_name)) {
        tlog_errs_pushf(perrs, "User name is not valid UTF-8: %s",
                        passwd->pw_name);
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }

    /* Get the terminal type */
    term = getenv("TERM");
    if (term == NULL) {
        term = "";
    }
    if (!tlog_utf8_str_is_valid(term)) {
        tlog_errs_pushf(perrs,
                        "TERM environment variable is not valid UTF-8: %s",
                        term);
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }

    /* Get the maximum payload size */
    if (!json_object_object_get_ex(conf, "payload", &obj)) {
        tlog_errs_pushs(perrs, "Maximum payload size is not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    num = json_object_get_int64(obj);

    /* Create the sink, letting it take over the writer */
    grc = tlog_json_sink_create(&sink, writer, true,
                                fqdn, passwd->pw_name, term,
                                session_id, (size_t)num);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating log sink");
        goto cleanup;
    }
    writer = NULL;

    *psink = sink;
    sink = NULL;
    grc = TLOG_RC_OK;
cleanup:

    if (fd >= 0) {
        close(fd);
    }
    tlog_json_writer_destroy(writer);
    free(fqdn);
    tlog_sink_destroy(sink);
    return grc;
}

/** Log data set item code */
enum tlog_log_item {
    TLOG_LOG_ITEM_INPUT = 0,
    TLOG_LOG_ITEM_OUTPUT,
    TLOG_LOG_ITEM_WINDOW
};

/** Get log data set item code for a (non-void) packet */
static enum tlog_log_item
tlog_log_item_from_pkt(const struct tlog_pkt *pkt)
{
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));
    return pkt->type == TLOG_PKT_TYPE_WINDOW
                ? TLOG_LOG_ITEM_WINDOW
                : (pkt->data.io.output
                        ? TLOG_LOG_ITEM_OUTPUT
                        : TLOG_LOG_ITEM_INPUT);
}

/**
 * Read a log mask from a configuration object.
 *
 * @param perrs Location for the error stack. Can be NULL.
 * @param conf  Log mask JSON configuration object.
 * @param pmask Location for the read mask.
 *
 * @return Global return code.
 */
static tlog_grc
get_log_mask(struct tlog_errs **perrs,
             struct json_object *conf,
             unsigned int *pmask)
{
    unsigned int mask = 0;
    struct json_object *obj;

    assert(conf != NULL);
    assert(pmask != NULL);

#define READ_ITEM(_NAME, _Name, _name) \
    do {                                                                \
        if (!json_object_object_get_ex(conf, #_name, &obj)) {           \
            tlog_errs_pushf(perrs,                                      \
                            "%s logging flag is not specified",         \
                            #_Name);                                    \
            return TLOG_RC_FAILURE;                                     \
        }                                                               \
        mask |= json_object_get_boolean(obj) << TLOG_LOG_ITEM_##_NAME;  \
    } while (0)

    READ_ITEM(INPUT, Input, input);
    READ_ITEM(OUTPUT, Output, output);
    READ_ITEM(WINDOW, Window, window);

#undef READ_ITEM

    *pmask = mask;
    return TLOG_RC_OK;
}

/**
 * Transfer and log terminal data until interrupted or either end closes.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param tty_source    TTY data source.
 * @param log_sink      Log sink.
 * @param tty_sink      TTY data sink.
 * @param latency       Number of seconds to wait before flushing logged data.
 * @param log_mask      Logging mask with bits indexed by enum tlog_log_item.
 *
 * @return Global return code.
 */
static tlog_grc
transfer(struct tlog_errs     **perrs,
         struct tlog_source    *tty_source,
         struct tlog_sink      *log_sink,
         struct tlog_sink      *tty_sink,
         unsigned int           latency,
         unsigned int           log_mask)
{
    const int exit_sig[] = {SIGINT, SIGTERM, SIGHUP};
    tlog_grc return_grc = TLOG_RC_OK;
    tlog_grc grc;
    size_t i, j;
    struct sigaction sa;
    bool alarm_set = false;
    bool log_pending = false;
    sig_atomic_t last_alarm_caught = 0;
    sig_atomic_t new_alarm_caught;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    struct tlog_pkt_pos tty_pos = TLOG_PKT_POS_VOID;
    struct tlog_pkt_pos log_pos = TLOG_PKT_POS_VOID;

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN) {
            sa.sa_handler = exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < TLOG_ARRAY_SIZE(exit_sig); j++) {
                sigaddset(&sa.sa_mask, exit_sig[j]);
            }
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
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed flushing log");
                return_grc = grc;
                goto cleanup;
            }
            last_alarm_caught = new_alarm_caught;
            log_pending = false;
        } else if (log_pending && !alarm_set) {
            alarm(latency);
            alarm_set = true;
        }

        /* Deliver logged data if any */
        if (tlog_pkt_pos_cmp(&tty_pos, &log_pos) < 0) {
            grc = tlog_sink_write(tty_sink, &pkt, &tty_pos, &log_pos);
            if (grc != TLOG_RC_OK) {
                if (grc == TLOG_GRC_FROM(errno, EINTR)) {
                    continue;
                } else if (grc != TLOG_GRC_FROM(errno, EBADF) &&
                           grc != TLOG_GRC_FROM(errno, EINVAL)) {
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs, "Failed writing terminal data");
                    return_grc = grc;
                }
                break;
            }
            continue;
        }

        /* Log the received data, if any */
        if (tlog_pkt_pos_is_in(&log_pos, &pkt)) {
            /* If asked to log this type of packet */
            if (log_mask & (1 << tlog_log_item_from_pkt(&pkt))) {
                grc = tlog_sink_write(log_sink, &pkt, &log_pos, NULL);
                if (grc != TLOG_RC_OK &&
                    grc != TLOG_GRC_FROM(errno, EINTR)) {
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs, "Failed logging terminal data");
                    return_grc = grc;
                    goto cleanup;
                }
                log_pending = true;
            } else {
                tlog_pkt_pos_move_past(&log_pos, &pkt);
            }
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
            } else if (grc != TLOG_GRC_FROM(errno, EBADF) &&
                       grc != TLOG_GRC_FROM(errno, EIO)) {
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed reading terminal data");
                return_grc = grc;
            }
            break;
        } else if (tlog_pkt_is_void(&pkt)) {
            break;
        }
    }

    /* Cut the log (write incomplete characters as binary) */
    grc = tlog_sink_cut(log_sink);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed cutting-off the log");
        if (return_grc == TLOG_RC_OK) {
            return_grc = grc;
        }
        goto cleanup;
    }

    /* Flush the log */
    grc = tlog_sink_flush(log_sink);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed flushing the log");
        if (return_grc == TLOG_RC_OK) {
            return_grc = grc;
        }
        goto cleanup;
    }

cleanup:

    tlog_pkt_cleanup(&pkt);
    /* Stop the timer */
    alarm(0);
    /* Restore signal handlers */
    signal(SIGALRM, SIG_DFL);
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN) {
            signal(exit_sig[i], SIG_DFL);
        }
    }

    return return_grc;
}

/**
 * Run tlog-rec-session with specified configuration and environment.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param euid      The effective UID the program was started with.
 * @param egid      The effective GID the program was started with.
 * @param cmd_help  Command-line usage help message.
 * @param conf      Tlog-rec-session configuration JSON object.
 * @param path      Path to the recorded program to execute.
 * @param argv      ARGV array for the recorded program.
 * @param in_fd     Stdin to connect to, or -1 if none.
 * @param out_fd    Stdout to connect to, or -1 if none.
 * @param err_fd    Stderr to connect to, or -1 if none.
 * @param pstatus   Location for the shell process status as returned by
 *                  waitpid(2), can be NULL, if not required.
 *                  Not modified in case of error.
 *
 * @return Global return code.
 */
static tlog_grc
run(struct tlog_errs **perrs, uid_t euid, gid_t egid,
    const char *cmd_help, struct json_object *conf,
    const char *path, char **argv,
    int in_fd, int out_fd, int err_fd,
    int *pstatus)
{
    tlog_grc grc;
    unsigned int session_id;
    bool lock_acquired = false;
    struct json_object *obj;
    int64_t num;
    unsigned int latency;
    unsigned int log_mask;
    struct tlog_sink *log_sink = NULL;
    struct tlog_tap tap = TLOG_TAP_VOID;

    assert(cmd_help != NULL);

    /* Check for the help flag */
    if (json_object_object_get_ex(conf, "help", &obj)) {
        if (json_object_get_boolean(obj)) {
            fprintf(stdout, "%s\n", cmd_help);
            grc = TLOG_RC_OK;
            goto cleanup;
        }
    }

    /* Check for the version flag */
    if (json_object_object_get_ex(conf, "version", &obj)) {
        if (json_object_get_boolean(obj)) {
            printf("%s", tlog_version);;
            grc = TLOG_RC_OK;
            goto cleanup;
        }
    }

    /* Get session ID */
    grc = tlog_session_get_id(&session_id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving session ID");
        goto cleanup;
    }

    /* Attempt to lock the session */
    grc = tlog_session_lock(perrs, session_id, euid, egid, &lock_acquired);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed locking session");
        goto cleanup;
    }

    /* If the session is already locked (recorded) */
    if (!lock_acquired) {
        /* Exec the bare session */
        grc = tlog_unpriv_execv(perrs, path, argv);
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed executing shell");
        goto cleanup;
    }

    /* Read the log latency */
    if (!json_object_object_get_ex(conf, "latency", &obj)) {
        tlog_errs_pushs(perrs, "Log latency is not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    num = json_object_get_int64(obj);
    latency = (unsigned int)num;

    /* Read log mask */
    if (!json_object_object_get_ex(conf, "log", &obj)) {
        tlog_errs_pushs(perrs,
                        "Logged data set parameters are not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    grc = get_log_mask(perrs, obj, &log_mask);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed reading log mask");
        goto cleanup;
    }

    /* Create the log sink */
    grc = create_log_sink(perrs, &log_sink, conf, session_id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed creating log sink");
        goto cleanup;
    }

    /* Output and discard any accumulated non-critical error messages */
    tlog_errs_print(stderr, *perrs);
    tlog_errs_destroy(perrs);

    /* Read and output the notice */
    if (json_object_object_get_ex(conf, "notice", &obj)) {
        fprintf(stderr, "%s", json_object_get_string(obj));
    }

    /* Setup the tap */
    grc = tlog_tap_setup(perrs, &tap, euid, egid, path, argv,
                         in_fd, out_fd, err_fd);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed setting up the I/O tap");
        goto cleanup;
    }

    /* Transfer and log the data until interrupted or either end is closed */
    grc = transfer(perrs, tap.source, log_sink, tap.sink, latency, log_mask);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed transferring TTY data");
        goto cleanup;
    }

    grc = TLOG_RC_OK;

cleanup:

    tlog_tap_teardown(perrs, &tap, (grc == TLOG_RC_OK ? pstatus : NULL));
    tlog_sink_destroy(log_sink);
    if (lock_acquired) {
        tlog_session_unlock(perrs, session_id, euid, egid);
    }
    return grc;
}

int
main(int argc, char **argv)
{
    tlog_grc grc;
    uid_t euid;
    gid_t egid;
    struct tlog_errs *errs = NULL;
    struct json_object *conf = NULL;
    char *cmd_help = NULL;
    int std_fds[] = {0, 1, 2};
    const char *charset;
    const char *shell_path;
    char **shell_argv = NULL;
    int status = 1;

    /* Remember effective UID/GID so we can return to them later */
    euid = geteuid();
    egid = getegid();

    /* Drop the privileges temporarily in case we're SUID/SGID */
    if (seteuid(getuid()) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs, "Failed setting EUID");
        goto cleanup;
    }
    if (setegid(getgid()) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs, "Failed setting EGID");
        goto cleanup;
    }

    /* Check if stdin/stdout/stderr are closed and stub them with /dev/null */
    /* NOTE: Must be done first to avoid FD takeover by other code */
    while (true) {
        int fd = open("/dev/null", O_RDWR);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(&errs, grc);
            tlog_errs_pushs(&errs, "Failed opening /dev/null");
            goto cleanup;
        } else if (fd >= (int)TLOG_ARRAY_SIZE(std_fds)) {
            close(fd);
            break;
        } else {
            std_fds[fd] = -1;
        }
    }

    /* Set locale from environment variables */
    if (setlocale(LC_ALL, "") == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs,
                        "Failed setting locale from environment variables");
        goto cleanup;
    }

    /* Read configuration and command-line usage message */
    grc = tlog_rec_session_conf_load(&errs, &cmd_help, &conf, argc, argv);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(&errs, "Failed retrieving configuration");
        goto cleanup;
    }

    /* Check that the character encoding is supported */
    charset = nl_langinfo(CODESET);
    if (strcmp(charset, "UTF-8") != 0) {
        grc = TLOG_RC_FAILURE;
        tlog_errs_pushf(&errs, "Unsupported locale charset: %s", charset);
        goto cleanup;
    }

    /* Prepare shell command line */
    grc = tlog_rec_session_conf_get_shell(&errs, conf,
                                          &shell_path, &shell_argv);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(&errs, "Failed building shell command line");
        goto cleanup;
    }

    /*
     * Set the SHELL environment variable to the actual shell. Otherwise
     * programs trying to spawn the user's shell would start tlog-rec-session
     * instead.
     */
    if (setenv("SHELL", shell_path, /*overwrite=*/1) != 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs, "Failed to set SHELL environment variable");
        goto cleanup;
    }

    /* Run */
    grc = run(&errs, euid, egid, cmd_help, conf, shell_path, shell_argv,
              std_fds[0], std_fds[1], std_fds[2],
              &status);

cleanup:

    /* Print error stack, if any */
    tlog_errs_print(stderr, errs);

    /* Free shell argv list */
    if (shell_argv != NULL) {
        size_t i;
        for (i = 0; shell_argv[i] != NULL; i++) {
            free(shell_argv[i]);
        }
        free(shell_argv);
    }

    json_object_put(conf);
    free(cmd_help);
    tlog_errs_destroy(&errs);

    /* Reproduce the exit signal to get proper exit status */
    if (exit_signum != 0) {
        raise(exit_signum);
    }

    /* Signal error if tlog error occurred */
    if (grc != TLOG_RC_OK) {
        return 1;
    }

    /* Mimick Bash exit status generation */
    return WIFSIGNALED(status)
                ? 128 + WTERMSIG(status)
                : WEXITSTATUS(status);
}
