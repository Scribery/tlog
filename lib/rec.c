/*
 * Generic recording process
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

#include <tlog/rec.h>
#include <tlog/rec_item.h>
#include <tlog/json_sink.h>
#include <tlog/syslog_json_writer.h>
#include <tlog/journal_json_writer.h>
#include <tlog/fd_json_writer.h>
#include <tlog/source.h>
#include <tlog/syslog_misc.h>
#include <tlog/session.h>
#include <tlog/tap.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pwd.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <tlog/pkt.h>

/**< Number of the signal causing exit */
static volatile sig_atomic_t tlog_rec_exit_signum;

static void
tlog_rec_exit_sighandler(int signum)
{
    if (tlog_rec_exit_signum == 0) {
        tlog_rec_exit_signum = signum;
    }
}

/* Number of ALRM signals caught */
static volatile sig_atomic_t tlog_rec_alarm_caught;

static void
tlog_rec_alarm_sighandler(int signum)
{
    (void)signum;
    tlog_rec_alarm_caught++;
}

/**
 * Get fully-qualified name of this host.
 *
 * @param pfqdn     Location for the dynamically-allocated name.
 *
 * @return Status code.
 */
static tlog_grc
tlog_rec_get_fqdn(char **pfqdn)
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
tlog_rec_create_log_sink(struct tlog_errs **perrs,
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
    } else if (strcmp(str, "journal") == 0) {
        struct json_object *conf_journal;
        int priority;

        /* Get journal writer conf container */
        if (!json_object_object_get_ex(conf, "journal", &conf_journal)) {
            tlog_errs_pushs(perrs, "Journal writer parameters are not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get priority */
        if (!json_object_object_get_ex(conf_journal, "priority", &obj)) {
            tlog_errs_pushs(perrs, "Journal priority is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);
        priority = tlog_syslog_priority_from_str(str);
        if (priority < 0) {
            tlog_errs_pushf(perrs, "Unknown journal priority: %s", str);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Create the writer */
        grc = tlog_journal_json_writer_create(&writer, priority,
                                              passwd->pw_name, session_id);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating journal writer");
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
    grc = tlog_rec_get_fqdn(&fqdn);
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

/**
 * Transfer and log terminal data until interrupted or either end closes.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param tty_source    TTY data source.
 * @param log_sink      Log sink.
 * @param tty_sink      TTY data sink.
 * @param latency       Number of seconds to wait before flushing logged data.
 * @param item_mask     Logging mask with bits indexed by enum tlog_rec_item.
 * @param psignal       Location for the number of signal which caused
 *                      transfer termination, or for zero, if terminated for
 *                      other reason. Not modified in case of error.
 *                      Can be NULL, if not needed.
 * @param cutoff_rate   Maximum rate accepted if use_rate is true; 0 disables feature.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_transfer(struct tlog_errs    **perrs,
                  struct tlog_source   *tty_source,
                  struct tlog_sink     *log_sink,
                  struct tlog_sink     *tty_sink,
                  unsigned int          latency,
                  unsigned              item_mask,
                  int                  *psignal,
                  uint64_t              cutoff_rate)
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
    struct timespec prev_time, cur_time;
    double d_rate= -1.0;
    uint64_t cur_rate = 0;
    uint64_t time_change;

    tlog_rec_exit_signum = 0;
    tlog_rec_alarm_caught = 0;

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN) {
            sa.sa_handler = tlog_rec_exit_sighandler;
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
    sa.sa_handler = tlog_rec_alarm_sighandler;
    sigemptyset(&sa.sa_mask);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    /*
     * Transfer I/O and window changes
     */
    while (tlog_rec_exit_signum == 0) {
        /* Handle latency limit */
        new_alarm_caught = tlog_rec_alarm_caught;
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
            if (item_mask & (1 << tlog_rec_item_from_pkt(&pkt))) {
                if(pkt.type == TLOG_PKT_TYPE_IO && cutoff_rate != 0){

                    /* Get the rate as an integer with decimal precision */
                    d_rate = (double)(pkt.data.io.len)/(double)(time_change);
                    cur_rate = d_rate * 1000000;

                    if(cur_rate < cutoff_rate && d_rate > 0.0){
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
                } else{
                    grc = tlog_sink_write(log_sink, &pkt, &log_pos, NULL);

                    if (grc != TLOG_RC_OK &&
                        grc != TLOG_GRC_FROM(errno, EINTR)) {
                        tlog_errs_pushc(perrs, grc);
                        tlog_errs_pushs(perrs, "Failed logging terminal data");
                        return_grc = grc;
                        goto cleanup;
                    }
                    log_pending = true;
                }
            } else {
                tlog_pkt_pos_move_past(&log_pos, &pkt);
            }
            continue;
        }

        /* Read new data */
        tlog_pkt_cleanup(&pkt);
        log_pos = TLOG_PKT_POS_VOID;
        tty_pos = TLOG_PKT_POS_VOID;

        /*Start timer for timing read*/
        clock_gettime(CLOCK_MONOTONIC, &prev_time);

        grc = tlog_source_read(tty_source, &pkt);

        /*End timer for timing read*/
        clock_gettime(CLOCK_MONOTONIC, &cur_time);

        /*Time elapsed in micro-seconds*/
        time_change = (cur_time.tv_sec - prev_time.tv_sec) * 1000000 + (cur_time.tv_nsec - prev_time.tv_nsec)/1000;

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

    if (psignal != NULL) {
        *psignal = tlog_rec_exit_signum;
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
 * Read a recorded item mask from a configuration object.
 *
 * @param perrs Location for the error stack. Can be NULL.
 * @param conf  Recorded item mask JSON configuration object.
 * @param pmask Location for the read mask.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_get_item_mask(struct tlog_errs **perrs,
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
        mask |= json_object_get_boolean(obj) << TLOG_REC_ITEM_##_NAME;  \
    } while (0)

    READ_ITEM(INPUT, Input, input);
    READ_ITEM(OUTPUT, Output, output);
    READ_ITEM(WINDOW, Window, window);

#undef READ_ITEM

    *pmask = mask;
    return TLOG_RC_OK;
}

tlog_grc
tlog_rec(struct tlog_errs **perrs, uid_t euid, gid_t egid,
         const char *cmd_help, struct json_object *conf,
         unsigned int opts, const char *path, char **argv,
         int in_fd, int out_fd, int err_fd,
         int *pstatus, int *psignal)
{
    tlog_grc grc;
    unsigned int session_id;
    bool lock_acquired = false;
    struct json_object *obj;
    int64_t num;
    unsigned int latency;
    uint64_t check_rate;
    unsigned int item_mask;
    int signal = 0;
    struct tlog_sink *log_sink = NULL;
    struct tlog_tap tap = TLOG_TAP_VOID;

    assert(cmd_help != NULL);

    /* Check for the help flag */
    if (json_object_object_get_ex(conf, "help", &obj)) {
        if (json_object_get_boolean(obj)) {
            fprintf(stdout, "%s\n", cmd_help);
            goto exit;
        }
    }

    /* Check for the version flag */
    if (json_object_object_get_ex(conf, "version", &obj)) {
        if (json_object_get_boolean(obj)) {
            printf("%s", tlog_version);;
            goto exit;
        }
    }

    /* Get session ID */
    grc = tlog_session_get_id(&session_id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving session ID");
        goto cleanup;
    }

    if (opts & TLOG_REC_OPT_LOCK_SESS) {
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
            grc = tlog_exec(perrs, opts & TLOG_EXEC_OPT_MASK, path, argv);
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed executing the program to record");
            goto cleanup;
        }
    }

    /* Read the log latency */
    if (!json_object_object_get_ex(conf, "latency", &obj)) {
        tlog_errs_pushs(perrs, "Log latency is not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    num = json_object_get_int64(obj);
    latency = (unsigned int)num;

    /* Read the rate limit*/
    if (!json_object_object_get_ex(conf, "rate", &obj)) {
        tlog_errs_pushs(perrs, "Rate is not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    check_rate = json_object_get_int64(obj);

    /* Read item mask */
    if (!json_object_object_get_ex(conf, "log", &obj)) {
        tlog_errs_pushs(perrs,
                        "Logged data set parameters are not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    grc = tlog_rec_get_item_mask(perrs, obj, &item_mask);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed reading log mask");
        goto cleanup;
    }

    /* Create the log sink */
    grc = tlog_rec_create_log_sink(perrs, &log_sink, conf, session_id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed creating log sink");
        goto cleanup;
    }

    /* Output and discard any accumulated non-critical error messages */
    tlog_errs_print(stderr, *perrs);
    tlog_errs_destroy(perrs);

    /* Read and output the notice, if any */
    if (json_object_object_get_ex(conf, "notice", &obj)) {
        fprintf(stderr, "%s", json_object_get_string(obj));
    }

    /* Setup the tap */
    grc = tlog_tap_setup(perrs, &tap, euid, egid,
                         opts & TLOG_EXEC_OPT_MASK, path, argv,
                         in_fd, out_fd, err_fd);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed setting up the I/O tap");
        goto cleanup;
    }

    /* Transfer and log the data until interrupted or either end is closed */
    grc = tlog_rec_transfer(perrs, tap.source, log_sink, tap.sink,
                            latency, item_mask, &signal, check_rate);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed transferring TTY data");
        goto cleanup;
    }

exit:
    if (psignal != NULL) {
        *psignal = signal;
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
