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

#include <config.h>
#include <tlog/rec.h>
#include <tlog/rec_item.h>
#include <tlog/json_sink.h>
#include <tlog/syslog_json_writer.h>
#ifdef TLOG_JOURNAL_ENABLED
#include <tlog/journal_json_writer.h>
#endif
#include <tlog/fd_json_writer.h>
#include <tlog/rl_json_writer.h>
#include <tlog/source.h>
#include <tlog/syslog_misc.h>
#include <tlog/session.h>
#include <tlog/tap.h>
#include <tlog/timespec.h>
#include <tlog/delay.h>
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
#include <ctype.h>
#include <assert.h>

/**< Number of the signal causing exit */
static volatile sig_atomic_t tlog_rec_exit_signum;

static void
tlog_rec_exit_sighandler(int signum)
{
    if (tlog_rec_exit_signum == 0) {
        tlog_rec_exit_signum = signum;
    }
}

/* True if an alarm was set, but not yet delivered */
static volatile sig_atomic_t tlog_rec_alarm_set;

/* Number of ALRM signals caught */
static volatile sig_atomic_t tlog_rec_alarm_caught;

/* True if child stopped or terminated */
static volatile sig_atomic_t tlog_rec_child_exited;

static void
tlog_rec_alarm_sighandler(int signum)
{
    (void)signum;
    tlog_rec_alarm_set = false;
    tlog_rec_alarm_caught++;
}

static void
tlog_rec_sigchld_handler()
{
    tlog_rec_child_exited = true;
}

/**
 * Get fully-qualified name of this host.
 *
 * @param pfqdn     Location for the dynamically-allocated name.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_get_fqdn(struct tlog_errs **perrs, char **pfqdn)
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
        if (gai_error == EAI_NONAME) {
            *pfqdn = strdup("");
            tlog_errs_pushs(perrs, "Host FQDN resolution failure");
            tlog_errs_pushs(perrs, "Falling back to empty hostname value");
            return TLOG_RC_OK;
        }
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
 * Get the ID of the recording.
 *
 * @param perrs Location for the error stack. Can be NULL.
 * @param pid   Location for the dynamically-allocated recording ID.
 *              Can be NULL.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_get_id(struct tlog_errs **perrs, char **pid)
{
    tlog_grc grc;
    char buf[256];
    char *p = buf;
    ssize_t l = sizeof(buf);
    const char *s;
    ssize_t rc;
    const char *boot_id_path = "/proc/sys/kernel/random/boot_id";
    const char *self_stat_path = "/proc/self/stat";
    FILE *file = NULL;
    unsigned long long int starttime;

    /*
     * Append boot ID
     */

    /* Open boot ID file */
    file = fopen(boot_id_path, "r");
    if (file == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed opening boot ID file %s",
                          boot_id_path);
    }

    /* Read the boot ID file */
    rc = fread(p, 1, l, file);
    if (rc == 0) {
        if (ferror(file)) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECF(grc, "Failed reading boot ID file %s",
                              boot_id_path);
        } else {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISECF(grc, "Boot ID file %s is empty",
                              boot_id_path);
        }
    }

    /* Close the boot ID file */
    fclose(file);
    file = NULL;

    /* Compress boot ID to just numbers */
    for (s = p; rc > 0 && l > 1; s++, rc--) {
        if (isxdigit(*s)) {
            *p = *s;
            p++;
            l--;
        } else if (*s != '-') {
            break;
        }
    }

    /*
     * Append separating dash and PID
     */
    rc = snprintf(p, l, "-%x", (unsigned int)getpid());
    if (rc >= l) {
        grc = TLOG_RC_FAILURE;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Generated recording ID is too long");
    }
    p += rc;
    l -= rc;

    /*
     * Append separating dash and process start time (in jiffies since boot)
     */
    /* Open the process stat file */
    file = fopen(self_stat_path, "r");
    if (file == NULL) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed opening process status file %s",
                          self_stat_path);
    }

    /* Read the process start time in jiffies since boot */
    /* Format specifiers are taken from proc(5) */
    rc = fscanf(file,
                "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u "
                "%*u %*u %*d %*d %*d %*d %*d %*d %llu", &starttime);
    if (rc == EOF) {
        if (ferror(file)) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECF(grc, "Failed reading process status file %s",
                              self_stat_path);
        } else {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISECF(grc, "Process status file %s is empty",
                              self_stat_path);
        }
    } else if (rc < 1) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISECF(grc, "Invalid format of process status file %s",
                          self_stat_path);
    }

    /* Close the process stat file */
    fclose(file);
    file = NULL;

    /* Append a separating dash and the start time */
    rc = snprintf(p, l, "-%llx", starttime);
    if (rc >= l) {
        grc = TLOG_RC_FAILURE;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Generated recording ID is too long");
    }
    p += rc;

    /*
     * Output, if requested.
     */
    if (pid != NULL) {
        *pid = strndup(buf, p - buf);
        if (*pid == NULL) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed allocating recording ID");
        }
    }

    /* Success! */
    grc = TLOG_RC_OK;

cleanup:
    if (file != NULL) {
        fclose(file);
    }

    return grc;
}

/**
 * Create a file JSON message writer according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param pwriter       Location for the created writer pointer.
 * @param euid          EUID to use while opening the file.
 * @param egid          EGID to use while opening the file.
 * @param conf          Writer configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_create_file_json_writer(struct tlog_errs **perrs,
                                 struct tlog_json_writer **pwriter,
                                 uid_t euid, gid_t egid,
                                 struct json_object *conf)
{
    tlog_grc grc;
    int rc;
    struct json_object *obj;
    const char *str;
    struct tlog_json_writer *writer = NULL;
    int fd = -1;

    assert(pwriter != NULL);
    assert(conf != NULL);

    /* Get the file path */
    if (!json_object_object_get_ex(conf, "path", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Log file path is not specified");
    }
    str = json_object_get_string(obj);

    /* Open the file */
    TLOG_EVAL_WITH_EUID_EGID(euid, egid,
                             fd = open(str, O_WRONLY | O_CREAT | O_APPEND,
                                       S_IRUSR | S_IWUSR));
    if (fd < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed opening log file \"%s\"", str);
    }
    /* Get file flags */
    rc = fcntl(fd, F_GETFD);
    if (rc < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed getting log file descriptor flags");
    }
    /* Add FD_CLOEXEC to file flags */
    rc = fcntl(fd, F_SETFD, rc | FD_CLOEXEC);
    if (rc < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed setting log file descriptor flags");
    }

    /* Create the writer, letting it take over the FD */
    grc = tlog_fd_json_writer_create(&writer, fd, true);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating file writer");
    }
    fd = -1;

    *pwriter = writer;
    writer = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_writer_destroy(writer);
    if (fd >= 0) {
        close(fd);
    }
    return grc;
}

/**
 * Create a syslog JSON message writer according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param pwriter       Location for the created writer pointer.
 * @param conf          Writer configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_create_syslog_json_writer(struct tlog_errs **perrs,
                                   struct tlog_json_writer **pwriter,
                                   struct json_object *conf)
{
    tlog_grc grc;
    struct json_object *obj;
    const char *str;
    struct tlog_json_writer *writer = NULL;
    int facility;
    int priority;

    assert(pwriter != NULL);
    assert(conf != NULL);

    /* Get facility */
    if (!json_object_object_get_ex(conf, "facility", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Syslog facility is not specified");
    }
    str = json_object_get_string(obj);
    facility = tlog_syslog_facility_from_str(str);
    if (facility < 0) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Unknown syslog facility: %s", str);
    }

    /* Get priority */
    if (!json_object_object_get_ex(conf, "priority", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Syslog priority is not specified");
    }
    str = json_object_get_string(obj);
    priority = tlog_syslog_priority_from_str(str);
    if (priority < 0) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Unknown syslog priority: %s", str);
    }

    /* Create the writer */
    openlog("tlog", LOG_NDELAY, facility);
    grc = tlog_syslog_json_writer_create(&writer, priority);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating syslog writer");
    }

    *pwriter = writer;
    writer = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_writer_destroy(writer);
    return grc;
}

#ifdef TLOG_JOURNAL_ENABLED
/**
 * Create a journal JSON message writer according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param pwriter       Location for the created writer pointer.
 * @param conf          Configuration JSON object.
 * @param id            ID of the recording being created.
 * @param username      The name of the user being recorded.
 * @param session_id    The ID of the audit session being recorded.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_create_journal_json_writer(struct tlog_errs **perrs,
                                    struct tlog_json_writer **pwriter,
                                    struct json_object *conf,
                                    const char *id,
                                    const char *username,
                                    unsigned int session_id)
{
    tlog_grc grc;
    struct json_object *obj;
    const char *str;
    struct tlog_json_writer *writer = NULL;
    bool augment;
    int priority;

    assert(pwriter != NULL);
    assert(conf != NULL);
    assert(id != NULL);
    assert(username != NULL);

    /* Get the "augment" flag */
    if (json_object_object_get_ex(conf, "augment", &obj)) {
        augment = json_object_get_boolean(obj);
    } else {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("\"Augment\" flag is not specified");
    }

    /* Get priority */
    if (!json_object_object_get_ex(conf, "priority", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Journal priority is not specified");
    }
    str = json_object_get_string(obj);
    priority = tlog_syslog_priority_from_str(str);
    if (priority < 0) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Unknown journal priority: %s", str);
    }

    /* Create the writer */
    grc = tlog_journal_json_writer_create(&writer, priority, augment,
                                          id, username, session_id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating journal writer");
    }

    *pwriter = writer;
    writer = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_writer_destroy(writer);
    return grc;
}
#endif

/**
 * Create a rate-limiting JSON message writer, if configured.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param pwriter       Location of the "below" writer pointer to attach under
 *                      the rate-limiting writer, and for the created
 *                      rate-limiting writer pointer, if rate-limiting is
 *                      enabled. Otherwise the pointer stays unchanged.
 * @param conf          Rate-limiting configuration JSON object.
 * @param clock_id      The clock to use for rate-limiting.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_create_rl_json_writer(struct tlog_errs **perrs,
                               struct tlog_json_writer **pwriter,
                               struct json_object *conf,
                               clockid_t clock_id)
{
    tlog_grc grc;
    struct json_object *obj;
    const char *str;
    struct tlog_json_writer *writer = NULL;
    bool drop;
    uint64_t rate;
    uint64_t burst;

    assert(pwriter != NULL);
    assert(tlog_json_writer_is_valid(*pwriter));
    assert(conf != NULL);

    /* Get the action */
    if (!json_object_object_get_ex(conf, "action", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Logging limit action is not specified");
    }
    str = json_object_get_string(obj);

    if (strcasecmp(str, "pass") == 0) {
        grc = TLOG_RC_OK;
        goto cleanup;
    } else if (strcasecmp(str, "delay") == 0) {
        drop = false;
    } else if (strcasecmp(str, "drop") == 0) {
        drop = true;
    } else {
        assert(!"Unknown limit action");
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Unknown limit action is specified: %s", str);
    }

    /* Get the rate */
    if (!json_object_object_get_ex(conf, "rate", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Logging rate limit is not specified");
    }
    rate = json_object_get_int64(obj);

    /* Get the burst threshold */
    if (!json_object_object_get_ex(conf, "burst", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Logging burst threshold is not specified");
    }
    burst = json_object_get_int64(obj);

    /* Superimpose the writer, transfer ownership of below writer */
    grc = tlog_rl_json_writer_create(pwriter, *pwriter, true, clock_id,
                                     (size_t)rate, (size_t)burst, drop);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating rate-limiting writer");
    }

cleanup:
    tlog_json_writer_destroy(writer);
    return grc;
}

/**
 * Create a JSON message writer according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param pwriter       Location for the created writer pointer.
 * @param euid          EUID to use while accessing sensitive resources.
 * @param egid          EGID to use while accessing sensitive resources.
 * @param conf          Configuration JSON object.
 * @param id            ID of the recording being created.
 * @param username      The name of the user being recorded.
 * @param session_id    The ID of the audit session being recorded.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_create_json_writer(struct tlog_errs **perrs,
                            struct tlog_json_writer **pwriter,
                            uid_t euid, gid_t egid,
                            struct json_object *conf,
                            const char *id,
                            const char *username,
                            unsigned int session_id)
{
    tlog_grc grc;
    struct json_object *obj;
    const char *str;
    struct json_object *writer_conf;
    struct tlog_json_writer *writer = NULL;

    assert(pwriter != NULL);
    assert(conf != NULL);
    assert(id != NULL);
    assert(username != NULL);

    /* Unused without Journal */
    (void)session_id;

    /*
     * Create the terminating writer
     */
    if (!json_object_object_get_ex(conf, "writer", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Writer type is not specified");
    }
    str = json_object_get_string(obj);
    if (strcmp(str, "file") == 0) {
        /* Get file writer conf container */
        if (!json_object_object_get_ex(conf, str, &writer_conf)) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISES("File writer parameters are not specified");
        }

        /* Create file writer */
        grc = tlog_rec_create_file_json_writer(perrs, &writer,
                                               euid, egid, writer_conf);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
    } else if (strcmp(str, "syslog") == 0) {
        /* Get syslog writer conf container */
        if (!json_object_object_get_ex(conf, str, &writer_conf)) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISES("Syslog writer parameters are not specified");
        }

        /* Create syslog writer */
        grc = tlog_rec_create_syslog_json_writer(perrs, &writer, writer_conf);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
#ifdef TLOG_JOURNAL_ENABLED
    } else if (strcmp(str, "journal") == 0) {
        /* Get journal writer conf container */
        if (!json_object_object_get_ex(conf, str, &writer_conf)) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISES("Journal writer parameters are not specified");
        }

        /* Create journal writer */
        grc = tlog_rec_create_journal_json_writer(perrs, &writer, writer_conf,
                                                  id, username, session_id);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
#endif
    } else {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Unknown writer type: %s", str);
    }

    /*
     * Attach the rate-limiting writer, if requested
     */
    /* Get limit conf container */
    if (!json_object_object_get_ex(conf, "limit", &writer_conf)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Logging limit parameters are not specified");
    }

    /* Create rate-limiting writer */
    grc = tlog_rec_create_rl_json_writer(perrs, &writer, writer_conf,
                                         CLOCK_MONOTONIC);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    *pwriter = writer;
    writer = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_writer_destroy(writer);
    return grc;
}

/**
 * Create a log sink according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param psink         Location for the created sink pointer.
 * @param euid          EUID to use while accessing sensitive resources.
 * @param egid          EGID to use while accessing sensitive resources.
 * @param conf          Configuration JSON object.
 * @param session_id    The ID of the session being recorded.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_create_log_sink(struct tlog_errs **perrs,
                         struct tlog_sink **psink,
                         uid_t euid, gid_t egid,
                         struct json_object *conf,
                         unsigned int session_id)
{
    tlog_grc grc;
    int64_t num;
    struct json_object *obj;
    struct tlog_sink *sink = NULL;
    struct tlog_json_writer *writer = NULL;
    char *fqdn = NULL;
    char *id = NULL;
    struct passwd *passwd;
    const char *term;

    /* Get recording ID */
    grc = tlog_rec_get_id(perrs, &id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed generating recording ID");
    }

    /* Get real user entry */
    errno = 0;
    passwd = getpwuid(getuid());
    if (passwd == NULL) {
        if (errno == 0) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISES("User entry not found");
        } else {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed retrieving user entry");
        }
    }
    if (!tlog_utf8_str_is_valid(passwd->pw_name)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("User name is not valid UTF-8: %s",
                         passwd->pw_name);
    }

    /*
     * Create the writer
     */
    grc = tlog_rec_create_json_writer(perrs, &writer, euid, egid, conf,
                                      id, passwd->pw_name, session_id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed creating JSON message writer");
    }

    /*
     * Create the sink
     */
    /* Get host FQDN */
    grc = tlog_rec_get_fqdn(perrs, &fqdn);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed retrieving host FQDN");
    }
    if (!tlog_utf8_str_is_valid(fqdn)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Host FQDN is not valid UTF-8: %s", fqdn);
    }

    /* Get the terminal type */
    term = getenv("TERM");
    if (term == NULL) {
        term = "";
    }
    if (!tlog_utf8_str_is_valid(term)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("TERM environment variable is not valid UTF-8: %s",
                         term);
    }

    /* Get the maximum payload size */
    if (!json_object_object_get_ex(conf, "payload", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Maximum payload size is not specified");
    }
    num = json_object_get_int64(obj);

    /* Create the sink, letting it take over the writer */
    {
        struct tlog_json_sink_params params = {
            .writer = writer,
            .writer_owned = true,
            .hostname = fqdn,
            .recording = id,
            .username = passwd->pw_name,
            .terminal = term,
            .session_id = session_id,
            .chunk_size = num,
        };
        grc = tlog_json_sink_create(&sink, &params);
        if (grc != TLOG_RC_OK) {
            TLOG_ERRS_RAISECS(grc, "Failed creating log sink");
        }
    }
    writer = NULL;

    *psink = sink;
    sink = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_writer_destroy(writer);
    free(id);
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
                  int                   in_fd,
                  int                  *psignal)
{
    const int exit_sig[] = {SIGINT, SIGTERM, SIGHUP};
    tlog_grc return_grc = TLOG_RC_OK;
    tlog_grc grc = TLOG_RC_OK;
    size_t i, j;
    struct sigaction sa;
    bool log_pending = false;
    sig_atomic_t last_alarm_caught = 0;
    sig_atomic_t new_alarm_caught;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    struct tlog_pkt_pos tty_pos = TLOG_PKT_POS_VOID;
    struct tlog_pkt_pos log_pos = TLOG_PKT_POS_VOID;

    tlog_rec_exit_signum = 0;
    tlog_rec_alarm_set = false;
    tlog_rec_alarm_caught = 0;
    tlog_rec_child_exited = false;

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        if (sigaction(exit_sig[i], NULL, &sa) == -1) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc,
                              "Failed to retrieve an exit signal action");
        }
        if (sa.sa_handler != SIG_IGN) {
            sa.sa_handler = tlog_rec_exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < TLOG_ARRAY_SIZE(exit_sig); j++) {
                sigaddset(&sa.sa_mask, exit_sig[j]);
            }
            /* NOTE: no SA_RESTART on purpose */
            sa.sa_flags = 0;
            if(sigaction(exit_sig[i], &sa, NULL) == -1) {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc,
                                  "Failed to set an exit signal action");
            }
        }
    }

    /* Setup ALRM signal handler */
    sa.sa_handler = tlog_rec_alarm_sighandler;
    sigemptyset(&sa.sa_mask);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    if(sigaction(SIGALRM, &sa, NULL) == -1) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc,
                          "Failed to set a SIGALRM signal action");
    }

    /* Setup SIGCHLD signal handler */
    sa.sa_handler = tlog_rec_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    if(sigaction(SIGCHLD, &sa, NULL) == -1) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc,
                          "Failed to set a SIGCHLD signal action");
    }
    if (isatty(in_fd)) {
#ifdef HAVE_UTEMPTER
        raise(SIGCHLD);
#endif
    }

    /*
     * Transfer I/O and window changes
     */
    while (tlog_rec_exit_signum == 0) {
        /* Expected exit conditions */
        if (tlog_rec_child_exited) {
            if (grc == TLOG_GRC_FROM(errno, EIO) || (tlog_pkt_is_eof(&pkt))) {
                break;
            }
        }

        /* Handle latency limit */
        new_alarm_caught = tlog_rec_alarm_caught;
        if (new_alarm_caught != last_alarm_caught) {
            grc = tlog_sink_flush(log_sink);
            if (grc == TLOG_GRC_FROM(errno, EINTR)) {
                continue;
            } else if (grc != TLOG_RC_OK) {
                return_grc = grc;
                TLOG_ERRS_RAISECS(grc, "Failed flushing log");
            }
            last_alarm_caught = new_alarm_caught;
            log_pending = false;
        } else if (log_pending && !tlog_rec_alarm_set) {
            tlog_rec_alarm_set = true;
            alarm(latency);
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
                grc = tlog_sink_write(log_sink, &pkt, &log_pos, NULL);
                if (grc == TLOG_RC_OK) {
                    log_pending = true;
                } else if (grc != TLOG_GRC_FROM(errno, EINTR)) {
                    return_grc = grc;
                    TLOG_ERRS_RAISECS(grc, "Failed logging terminal data");
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
        } else if (tlog_pkt_is_eof(&pkt)) {
            tlog_sink_io_close(tty_sink, pkt.data.io.output);
        }
    }

    /* Cancel pending alarm, if any, to avoid interruptions */
    if (tlog_rec_alarm_set) {
        alarm(0);
        tlog_rec_alarm_set = false;
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
        if (grc == (TLOG_GRC_FROM(systemd, -ENOENT))) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Systemd journal not available or not responsive");
        } else {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed flushing the log");
        }
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
    /* Stop the timer, if any */
    if (tlog_rec_alarm_set) {
        alarm(0);
        tlog_rec_alarm_set = false;
    }
    /* Restore signal handlers */
    signal(SIGALRM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
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

/**
 * Signal through the "READY" semaphore file, if specified.
 *
 * @param perrs Location for the error stack. Can be NULL.
 * @param conf  Recording program configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_rec_semaphore(struct tlog_errs **perrs, struct json_object *conf)
{
    tlog_grc grc;
    struct json_object *obj;
    const char *str;
    int fd = -1;
    ssize_t rc;

    if (json_object_object_get_ex(conf, "semaphore", &obj)) {
        str = json_object_get_string(obj);
        fd = open(str, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECF(grc,
                              "Failed opening semaphore file \"%s\"", str);
        }
        rc = write(fd, "READY", 5);
        if (rc < 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECF(grc,
                              "Failed writing semaphore file \"%s\"", str);
        } else if (rc < 5) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISEF("Short write to semaphore file \"%s\"", str);
        }
        close(fd);
        fd = -1;
    }

    grc = TLOG_RC_OK;
cleanup:
    if (fd >= 0) {
        close(fd);
    }
    return grc;
}

tlog_grc
tlog_rec(struct tlog_errs **perrs, uid_t euid, gid_t egid,
         const char *cmd_help, struct json_object *conf,
         unsigned int opts, const char *path, char **argv,
         int in_fd, int out_fd, int err_fd,
         int *pstatus, int *psignal)
{
    tlog_grc grc;
    clockid_t clock_id;
    unsigned int session_id;
    bool lock_acquired = false;
    bool session_locking = true;
    struct json_object *obj;
    int64_t num;
    unsigned int latency;
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

    /* Check for the session_locking flag */
    if (json_object_object_get_ex(conf, "session_locking", &obj)) {
        session_locking = json_object_get_boolean(obj);
    }

    /* Check for the version flag */
    if (json_object_object_get_ex(conf, "version", &obj)) {
        if (json_object_get_boolean(obj)) {
            printf("%s", tlog_version);
            goto exit;
        }
    }

    /* Check for the configuration flag */
    if (json_object_object_get_ex(conf, "configuration", &obj)) {
        if (json_object_get_boolean(obj)) {
            const char *str;
            str = json_object_to_json_string_ext(conf,
                                                 JSON_C_TO_STRING_PRETTY);
            if (str == NULL) {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc,
                                  "Failed formatting configuration JSON");
            }
            printf("%s\n", str);
            grc = TLOG_RC_OK;
            goto cleanup;
        }
    }

    /*
     * Choose the clock: try to use coarse monotonic clock (which is faster),
     * if it provides the required resolution.
     */
    {
#ifdef CLOCK_MONOTONIC_COARSE
        struct timespec timestamp;
        if (clock_getres(CLOCK_MONOTONIC_COARSE, &timestamp) == 0 &&
            tlog_timespec_cmp(&timestamp, &tlog_delay_min_timespec) <= 0) {
            clock_id = CLOCK_MONOTONIC_COARSE;
        } else {
#endif
            if (clock_getres(CLOCK_MONOTONIC, NULL) == 0) {
                clock_id = CLOCK_MONOTONIC;
            } else {
                grc = TLOG_RC_FAILURE;
                TLOG_ERRS_RAISES("No clock to use");
            }
#ifdef CLOCK_MONOTONIC_COARSE
        }
#endif
    }

    /* Get session ID */
    grc = tlog_session_get_id(&session_id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed retrieving session ID");
    }

    if (opts & TLOG_REC_OPT_LOCK_SESS && session_locking) {
        /* Attempt to lock the session */
        grc = tlog_session_lock(perrs, session_id, euid, egid, &lock_acquired);
        if (grc != TLOG_RC_OK) {
            TLOG_ERRS_RAISES("Failed locking session");
        }

        /* If the session is already locked (recorded) */
        if (!lock_acquired) {
            /* Exec the bare session */
            tlog_exec(perrs, opts & TLOG_EXEC_OPT_MASK, path, argv);
            tlog_errs_print(stderr, *perrs);
            tlog_errs_destroy(perrs);
            _exit(127);
        }
    }

    /* Read the log latency */
    if (!json_object_object_get_ex(conf, "latency", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Log latency is not specified");
    }
    num = json_object_get_int64(obj);
    latency = (unsigned int)num;

    /* Read item mask */
    if (!json_object_object_get_ex(conf, "log", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Logged data set parameters are not specified");
    }
    grc = tlog_rec_get_item_mask(perrs, obj, &item_mask);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed reading log mask");
    }

    /* Create the log sink */
    grc = tlog_rec_create_log_sink(perrs, &log_sink, euid, egid, conf,
                                   session_id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed creating log sink");
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
                         in_fd, out_fd, err_fd, clock_id);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed setting up the I/O tap");
    }

    /* Signal "READY" semaphore, if specified */
    grc = tlog_rec_semaphore(perrs, conf);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed signalling \"READY\" semaphore");
    }

    /* Transfer and log the data until interrupted or either end is closed */
    grc = tlog_rec_transfer(perrs, tap.source, log_sink, tap.sink,
                            latency, item_mask, in_fd, &signal);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed transferring TTY data");
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
