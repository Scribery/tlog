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
#include <libgen.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <tlog/syslog_json_writer.h>
#include <tlog/fd_json_writer.h>
#include <tlog/tty_source.h>
#include <tlog/json_sink.h>
#include <tlog/tty_sink.h>
#include <tlog/syslog_misc.h>
#include <tlog/rc.h>
#include <tlog/rec_conf.h>
#include <tlog/rec_conf_cmd.h>

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

/**
 * Create the log sink according to configuration.
 *
 * @param psink Location for the created sink pointer.
 * @param conf  Configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
create_log_sink(struct tlog_sink **psink, struct json_object *conf)
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
    unsigned int session_id;

    /*
     * Create the writer
     */
    if (!json_object_object_get_ex(conf, "writer", &obj)) {
        fprintf(stderr, "Writer type is not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    str = json_object_get_string(obj);
    if (strcmp(str, "file") == 0) {
        struct json_object *conf_file;
        int fd;

        /* Get file writer conf container */
        if (!json_object_object_get_ex(conf, "file", &conf_file)) {
            fprintf(stderr, "File writer parameters are not specified\n");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get the file path */
        if (!json_object_object_get_ex(conf_file, "path", &obj)) {
            fprintf(stderr, "Log file path is not specified\n");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);

        /* Open the file */
        fd = open(str, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed opening log file \"%s\": %s\n",
                    str, tlog_grc_strerror(grc));
            goto cleanup;
        }

        /* Create the writer, letting it take over the FD */
        grc = tlog_fd_json_writer_create(&writer, fd, true);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr, "Failed creating file writer: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        fd = -1;
    } else if (strcmp(str, "syslog") == 0) {
        struct json_object *conf_syslog;
        int facility;
        int priority;

        /* Get syslog writer conf container */
        if (!json_object_object_get_ex(conf, "syslog", &conf_syslog)) {
            fprintf(stderr, "Syslog writer parameters are not specified\n");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get facility */
        if (!json_object_object_get_ex(conf_syslog, "facility", &obj)) {
            fprintf(stderr, "Syslog facility is not specified\n");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);
        facility = tlog_syslog_facility_from_str(str);
        if (facility < 0) {
            fprintf(stderr, "Unknown syslog facility: %s\n", str);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get priority */
        if (!json_object_object_get_ex(conf_syslog, "priority", &obj)) {
            fprintf(stderr, "Syslog priority is not specified\n");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        str = json_object_get_string(obj);
        priority = tlog_syslog_priority_from_str(str);
        if (priority < 0) {
            fprintf(stderr, "Unknown syslog priority: %s\n", str);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Create the writer */
        openlog("tlog", LOG_NDELAY, facility);
        grc = tlog_syslog_json_writer_create(&writer, priority);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr, "Failed creating syslog writer: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
    } else {
        fprintf(stderr, "Unknown writer type: %s\n", str);
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }

    /*
     * Create the sink
     */
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

    /* Get effective user entry */
    errno = 0;
    passwd = getpwuid(geteuid());
    if (passwd == NULL) {
        if (errno == 0) {
            grc = TLOG_RC_FAILURE;
            fprintf(stderr, "User entry not found\n");
        } else {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed retrieving user entry: %s\n",
                    tlog_grc_strerror(grc));
        }
        goto cleanup;
    }

    /* Get the maximum payload size */
    if (!json_object_object_get_ex(conf, "payload", &obj)) {
        fprintf(stderr, "Maximum payload size is not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    num = json_object_get_int64(obj);

    /* Create the sink, letting it take over the writer */
    grc = tlog_json_sink_create(&sink, writer, true,
                                fqdn, passwd->pw_name,
                                session_id, (size_t)num);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating log sink: %s\n",
                tlog_grc_strerror(grc));
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
 * @param conf  Log mask JSON configuration object.
 * @param pmask Location for the read mask.
 *
 * @return Global return code.
 */
static tlog_grc
get_log_mask(struct json_object *conf, unsigned int *pmask)
{
    unsigned int mask = 0;
    struct json_object *obj;

    assert(conf != NULL);
    assert(pmask != NULL);

#define READ_ITEM(_NAME, _Name, _name) \
    do {                                                                \
        if (!json_object_object_get_ex(conf, #_name, &obj)) {           \
            fprintf(stderr, #_Name " logging flag is not specified\n"); \
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
 * Get shell parameters from a configuration JSON object.
 *
 * @param conf  Configuration JSON object.
 * @param ppath Location for the shell path pointer, references the
 *              configuration object.
 * @param pargv Location for the NULL-terminated shell argv array, to be
 *              freed, both the array and the elements.
 *
 * @return Global return code.
 */
static tlog_grc
get_shell(struct json_object *conf, const char **ppath, char ***pargv)
{
    tlog_grc grc;
    struct json_object *obj;
    struct json_object *args;
    bool login;
    bool command;
    const char *path;
    char *name = NULL;
    char *buf = NULL;
    char **argv = NULL;
    size_t argi;
    char *arg = NULL;
    int i;

    /* Read the login flag */
    login = json_object_object_get_ex(conf, "login", &obj) &&
            json_object_get_boolean(obj);

    /* Read the shell path */
    if (!json_object_object_get_ex(conf, "shell", &obj)) {
        fprintf(stderr, "Shell is not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    path = json_object_get_string(obj);

    /* Format shell name */
    buf = strdup(path);
    if (buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed duplicating shell path: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    if (login) {
        const char *str = basename(buf);
        name = malloc(strlen(str) + 2);
        if (name == NULL) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed allocating shell name: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        name[0] = '-';
        strcpy(name + 1, str);
        free(buf);
    } else {
        name = buf;
    }
    buf = NULL;

    /* Read the command flag */
    command = json_object_object_get_ex(conf, "command", &obj) &&
              json_object_get_boolean(obj);

    /* Read and check the positional arguments */
    if (!json_object_object_get_ex(conf, "args", &args)) {
        fprintf(stderr, "Positional arguments are not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    if (command && json_object_array_length(args) == 0) {
        fprintf(stderr, "Command string is not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }

    /* Create and fill argv list */
    argv = calloc(1 + (command ? 1 : 0) + json_object_array_length(args) + 1,
                  sizeof(*argv));
    if (argv == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed allocating argv list: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    argi = 0;
    argv[argi++] = name;
    name = NULL;
    if (command) {
        arg = strdup("-c");
        if (arg == NULL) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed allocating shell argv[#%zu]: %s\n",
                    argi, tlog_grc_strerror(grc));
            goto cleanup;
        }
        argv[argi++] = arg;
        arg = NULL;
    }
    for (i = 0; i < json_object_array_length(args); i++, argi++) {
        obj = json_object_array_get_idx(args, i);
        arg = strdup(json_object_get_string(obj));
        if (arg == NULL) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed allocating shell argv[#%zu]: %s\n",
                    argi, tlog_grc_strerror(grc));
            goto cleanup;
        }
        argv[argi] = arg;
        arg = NULL;
    }

    /* Output results */
    *ppath = path;
    *pargv = argv;
    argv = NULL;
    grc = TLOG_RC_OK;
cleanup:

    free(arg);
    if (argv != NULL) {
        for (argi = 0; argv[argi] != NULL; argi++) {
            free(argv[argi]);
        }
        free(argv);
    }
    free(name);
    free(buf);
    return grc;
}

/**
 * Transfer and log terminal data until interrupted or either end closes.
 *
 * @param tty_source    TTY data source.
 * @param log_sink      Log sink.
 * @param tty_sink      TTY data sink.
 * @param latency       Number of seconds to wait before flushing logged data.
 * @param log_mask      Logging mask with bits indexed by enum tlog_log_item.
 *
 * @return Global return code.
 */
static tlog_grc
transfer(struct tlog_source    *tty_source,
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
                    fprintf(stderr, "Failed writing terminal data: %s\n",
                            tlog_grc_strerror(grc));
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
                    fprintf(stderr, "Failed logging terminal data: %s\n",
                            tlog_grc_strerror(grc));
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
                fprintf(stderr, "Failed reading terminal data: %s\n",
                        tlog_grc_strerror(grc));
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
        fprintf(stderr, "Failed cutting-off the log: %s\n",
                tlog_grc_strerror(grc));
        if (return_grc == TLOG_RC_OK) {
            return_grc = grc;
        }
        goto cleanup;
    }

    /* Flush the log */
    grc = tlog_sink_flush(log_sink);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed flushing the log: %s\n",
                tlog_grc_strerror(grc));
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
        if (sa.sa_handler != SIG_IGN)
            signal(exit_sig[i], SIG_DFL);
    }

    return return_grc;
}

static tlog_grc
run(const char *progname, struct json_object *conf)
{
    tlog_grc grc;
    struct json_object *obj;
    int64_t num;
    unsigned int latency;
    unsigned int log_mask;
    const char *shell_path;
    char **shell_argv = NULL;
    const struct timespec delay_min = TLOG_DELAY_MIN_TIMESPEC;
    clockid_t clock_id;
    struct timespec timestamp;
    struct tlog_sink *log_sink = NULL;
    struct tlog_sink *tty_sink = NULL;
    struct tlog_source *tty_source = NULL;
    ssize_t rc;
    int master_fd;
    pid_t child_pid;
    bool term_attrs_set = false;
    struct termios orig_termios;
    struct termios raw_termios;
    struct winsize winsize;

    assert(progname != NULL);

    /* Check for the help flag */
    if (json_object_object_get_ex(conf, "help", &obj)) {
        if (json_object_get_boolean(obj)) {
            tlog_rec_conf_cmd_help(stdout, progname);
            grc = TLOG_RC_OK;
            goto cleanup;
        }
    }

    /* Prepare shell command line */
    grc = get_shell(conf, &shell_path, &shell_argv);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed building shell command line: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Read the log latency */
    if (!json_object_object_get_ex(conf, "latency", &obj)) {
        fprintf(stderr, "Log latency is not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    num = json_object_get_int64(obj);
    latency = (unsigned int)num;

    /* Read log mask */
    if (!json_object_object_get_ex(conf, "log", &obj)) {
        fprintf(stderr, "Logged data set parameters are not specified\n");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    grc = get_log_mask(obj, &log_mask);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed reading log mask: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Create the log sink */
    grc = create_log_sink(&log_sink, conf);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating log sink: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /*
     * Choose the clock: try to use coarse monotonic clock (which is faster),
     * if it provides the required resolution.
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
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed forking a pty: %s\n", tlog_grc_strerror(grc));
        goto cleanup;
    } else if (child_pid == 0) {
        /*
         * Child
         */
        execv(shell_path, shell_argv);
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed executing the shell: %s",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /*
     * Parent
     */
    /* Read and output the notice */
    if (json_object_object_get_ex(conf, "notice", &obj)) {
        fprintf(stderr, "%s", json_object_get_string(obj));
    }

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
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed setting tty attributes: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    term_attrs_set = true;

    /* Create the TTY source */
    grc = tlog_tty_source_create(&tty_source, STDIN_FILENO,
                                 master_fd, STDOUT_FILENO,
                                 4096, clock_id);
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

    /* Transfer and log the data until interrupted or either end is closed */
    grc = transfer(tty_source, log_sink, tty_sink, latency, log_mask);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed transferring TTY data: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    grc = TLOG_RC_OK;

cleanup:

    tlog_sink_destroy(log_sink);
    tlog_sink_destroy(tty_sink);
    tlog_source_destroy(tty_source);

    /* Free shell argv list */
    if (shell_argv != NULL) {
        for (size_t i = 0; shell_argv[i] != NULL; i++) {
            free(shell_argv[i]);
        }
        free(shell_argv);
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

    return grc;
}

int
main(int argc, char **argv)
{
    tlog_grc grc;
    struct json_object *conf = NULL;
    char *progname = NULL;

    /* Read configuration and program name */
    grc = tlog_rec_conf_load(&progname, &conf, argc, argv);
    if (grc != TLOG_RC_OK) {
        return 1;
    }

    /* Run */
    grc = run(progname, conf);

    /* Free configuration and program name */
    json_object_put(conf);
    free(progname);

    /* Reproduce the exit signal to get proper exit status */
    if (exit_signum != 0)
        raise(exit_signum);

    return grc != TLOG_RC_OK;
}
