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
#include <tlog/tty_source.h>
#include <tlog/json_sink.h>
#include <tlog/tty_sink.h>
#include <tlog/syslog_misc.h>
#include <tlog/timespec.h>
#include <tlog/delay.h>
#include <tlog/rc.h>
#include <tlog/rec_conf.h>
#include <tlog/rec_conf_cmd.h>

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
 * Evaluate an expression with specified EUID/EGID set temporarily.
 *
 * In case of an error setting/restoring EUID/EGID, sets the "grc" variable,
 * pushes messages to the specified error stack and jumps to "cleanup" label.
 *
 * @param _perrs    Location for the error stack. Can be NULL.
 * @param _euid     The EUID to set temporarily.
 * @param _egid     The EGID to set temporarily.
 * @param _expr     The expression to evaluate with EUID/EGID set.
 */
#define EVAL_WITH_EUID_EGID(_perrs, _euid, _egid, _expr) \
    do {                                                        \
        struct tlog_errs **__perrs = (_perrs);                  \
        uid_t _orig_euid = geteuid();                           \
        gid_t _orig_egid = getegid();                           \
                                                                \
        /* Set EUID temporarily */                              \
        if (seteuid(_euid) < 0) {                               \
            grc = TLOG_GRC_ERRNO;                               \
            tlog_errs_pushc(__perrs, grc);                      \
            tlog_errs_pushs(__perrs, "Failed setting EUID");    \
            goto cleanup;                                       \
        }                                                       \
                                                                \
        /* Set EGID temporarily */                              \
        if (setegid(_egid) < 0) {                               \
            grc = TLOG_GRC_ERRNO;                               \
            tlog_errs_pushc(__perrs, grc);                      \
            tlog_errs_pushs(__perrs, "Failed setting EGID");    \
            goto cleanup;                                       \
        }                                                       \
                                                                \
        /* Evaluate */                                          \
        _expr;                                                  \
                                                                \
        /* Restore EUID */                                      \
        if (seteuid(_orig_euid) < 0) {                          \
            grc = TLOG_GRC_ERRNO;                               \
            tlog_errs_pushc(__perrs, grc);                      \
            tlog_errs_pushs(__perrs, "Failed restoring EUID");  \
            goto cleanup;                                       \
        }                                                       \
                                                                \
        /* Restore EGID */                                      \
        if (setegid(_orig_egid) < 0) {                          \
            grc = TLOG_GRC_ERRNO;                               \
            tlog_errs_pushc(__perrs, grc);                      \
            tlog_errs_pushs(__perrs, "Failed restoring EGID");  \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

/**
 * Get process session ID.
 *
 * @param pid   Location for the retrieved session ID.
 *
 * @return Global return code.
 */
static tlog_grc
session_get_id(unsigned int *pid)
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
session_format_lock_path(char **ppath, unsigned int id)
{
    tlog_grc grc;
    char *path;

    assert(ppath != NULL);

    if (asprintf(&path, TLOG_SESSION_LOCK_DIR "/session.%u.lock", id) < 0) {
        grc = TLOG_GRC_ERRNO;
        goto cleanup;
    }

    *ppath = path;
    grc = TLOG_RC_OK;
cleanup:
    return grc;
}

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
static tlog_grc
session_lock(struct tlog_errs **perrs, unsigned int id,
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
    grc = session_format_lock_path(&path, id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed formatting lock file path");
        goto cleanup;
    }

    /*
     * Attempt to create session lock file.
     * Assume session ID never repeats (never wraps around).
     * FIXME Handle repeating session IDs.
     */
    EVAL_WITH_EUID_EGID(perrs, euid, egid,
                        fd = open(path,
                                  O_CREAT | O_CLOEXEC | O_EXCL,
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
static tlog_grc
session_unlock(struct tlog_errs **perrs, unsigned int id,
               uid_t euid, gid_t egid)
{
    int rc;
    tlog_grc grc;
    char *path = NULL;

    /* Format lock file path */
    grc = session_format_lock_path(&path, id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed formatting lock file path");
        goto cleanup;
    }

    /* Remove the lock file */
    EVAL_WITH_EUID_EGID(perrs, euid, egid, rc = unlink(path));
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
        fd = open(str, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                  S_IRUSR | S_IWUSR);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed opening log file \"%s\"", str);
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
forkpipes(int *pin_fd, int *pout_fd)
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

/**
 * Execute the shell with path and arguments specified in tlog-rec
 * configuration.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param conf      Tlog-rec configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
shell_exec(struct tlog_errs **perrs,
           struct json_object *conf)
{
    tlog_grc grc;
    const char *path;
    char **argv = NULL;
    uid_t uid = getuid();
    gid_t gid = getgid();

    /* Prepare shell command line */
    grc = tlog_rec_conf_get_shell(perrs, conf, &path, &argv);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed building shell command line");
        goto cleanup;
    }

    /*
     * Set the SHELL environment variable to the actual shell. Otherwise
     * programs trying to spawn the user's shell would start tlog-rec
     * instead.
     */
    if (setenv("SHELL", path, /*overwrite=*/1) != 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed to set SHELL environment variable");
        goto cleanup;
    }
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

    /* Exec the shell */
    execv(path, argv);
    grc = TLOG_GRC_ERRNO;
    tlog_errs_pushc(perrs, grc);
    tlog_errs_pushf(perrs, "Failed executing %s", path);

cleanup:

    /* Free shell argv list */
    if (argv != NULL) {
        size_t i;
        for (i = 0; argv[i] != NULL; i++) {
            free(argv[i]);
        }
        free(argv);
    }

    return grc;
}

/** I/O tap state */
struct tap {
    pid_t               pid;            /**< Shell PID */
    struct tlog_source *source;         /**< TTY data source */
    struct tlog_sink   *sink;           /**< TTY data sink */
    int                 in_fd;          /**< Shell input FD */
    int                 out_fd;         /**< Shell output FD */
    int                 tty_fd;         /**< Controlling terminal FD, or -1 */
    struct termios      termios_orig;   /**< Original terminal attributes */
    bool                termios_set;    /**< True if terminal attributes were
                                             changed from the original */
};

/** A void I/O tap state initializer */
#define TAP_VOID \
    (struct tap) {      \
        .source = NULL, \
        .sink   = NULL, \
        .in_fd  = -1,   \
        .out_fd = -1,   \
        .tty_fd = -1,   \
    }

/**
 * Teardown an I/O tap state.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param tap       The tap state to teardown.
 * @param pstatus   Location for the shell process status as returned by
 *                  waitpid(2), can be NULL, if not required.
 *
 * @return Global return code.
 */
static tlog_grc
tap_teardown(struct tlog_errs **perrs, struct tap *tap, int *pstatus)
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
    }

    return TLOG_RC_OK;
}

/**
 * Setup I/O tap.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param ptap      Location for the tap state.
 * @param euid      The effective UID the program was started with.
 * @param egid      The effective GID the program was started with.
 * @param conf      Tlog-rec configuration JSON object.
 * @param in_fd     Stdin to connect to, or -1 if none.
 * @param out_fd    Stdout to connect to, or -1 if none.
 * @param err_fd    Stderr to connect to, or -1 if none.
 *
 * @return Global return code.
 */
static tlog_grc
tap_setup(struct tlog_errs **perrs,
          struct tap *ptap, uid_t euid, gid_t egid,
          struct json_object *conf,
          int in_fd, int out_fd, int err_fd)
{
    tlog_grc grc;
    struct tap tap = TAP_VOID;
    clockid_t clock_id;
    sem_t *sem = MAP_FAILED;
    bool sem_initialized = false;

    assert(ptap != NULL);

    /*
     * Choose the clock: try to use coarse monotonic clock (which is faster),
     * if it provides the required resolution.
     */
    {
        struct timespec timestamp;
#ifdef CLOCK_MONOTONIC_COARSE
        if (clock_getres(CLOCK_MONOTONIC_COARSE, &timestamp) == 0 &&
            tlog_timespec_cmp(&timestamp, &tlog_delay_min_timespec) <= 0) {
            clock_id = CLOCK_MONOTONIC_COARSE;
        } else {
#endif
            if (clock_getres(CLOCK_MONOTONIC, NULL) == 0) {
                clock_id = CLOCK_MONOTONIC;
            } else {
                tlog_errs_pushs(perrs, "No clock to use");
                grc = TLOG_RC_FAILURE;
                goto cleanup;
            }
#ifdef CLOCK_MONOTONIC_COARSE
        }
#endif
    }

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
        tap.pid = forkpipes(in_fd >= 0 ? &tap.in_fd : NULL,
                            out_fd >= 0 ? &tap.out_fd : NULL);
        if (tap.pid < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs,
                            "Failed forking a child connected via pipes");
            goto cleanup;
        }
    }

    /* Execute the shell in the child */
    if (tap.pid == 0) {
        /* Wait for the parent to lock the privileges */
        if (sem_wait(sem) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs,
                            "Failed waiting for parent to lock privileges");
            goto cleanup;
        }

        /* Execute the shell */
        grc = shell_exec(perrs, conf);
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed executing shell");
        goto cleanup;
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
    tap = TAP_VOID;

cleanup:

    if (sem_initialized) {
        sem_destroy(sem);
    }
    if (sem != MAP_FAILED) {
        munmap(sem, sizeof(*sem));
    }
    tap_teardown(perrs, &tap, NULL);

    return grc;
}

/**
 * Run tlog-rec with specified configuration and environment.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param euid      The effective UID the program was started with.
 * @param egid      The effective GID the program was started with.
 * @param cmd_help  Command-line usage help message.
 * @param conf      Tlog-rec configuration JSON object.
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
    struct tap tap = TAP_VOID;

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
    grc = session_get_id(&session_id);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving session ID");
        goto cleanup;
    }

    /* Attempt to lock the session */
    grc = session_lock(perrs, session_id, euid, egid, &lock_acquired);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed locking session");
        goto cleanup;
    }

    /* If the session is already locked (recorded) */
    if (!lock_acquired) {
        /* Exec the bare session */
        grc = shell_exec(perrs, conf);
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
    grc = tap_setup(perrs, &tap, euid, egid, conf, in_fd, out_fd, err_fd);
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

    tap_teardown(perrs, &tap, (grc == TLOG_RC_OK ? pstatus : NULL));
    tlog_sink_destroy(log_sink);
    if (lock_acquired) {
        session_unlock(perrs, session_id, euid, egid);
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
    int status;

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
    grc = tlog_rec_conf_load(&errs, &cmd_help, &conf, argc, argv);
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

    /* Run */
    grc = run(&errs, euid, egid, cmd_help, conf,
              std_fds[0], std_fds[1], std_fds[2],
              &status);

cleanup:

    /* Print error stack, if any */
    tlog_errs_print(stderr, errs);

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
