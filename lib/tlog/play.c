/*
 * Generic playback process
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
#include <tlog/play.h>
#ifdef TLOG_JOURNAL_ENABLED
#include <tlog/journal_json_reader.h>
#include <tlog/journal_misc.h>
#endif
#include <tlog/fd_json_reader.h>
#include <tlog/es_json_reader.h>
#include <tlog/json_source.h>
#include <tlog/timestr.h>
#include <tlog/timespec.h>
#include <curl/curl.h>
#include <termios.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define POLL_PERIOD 1

/**< Number of the signal causing exit */
static volatile sig_atomic_t tlog_play_exit_signum;

static void
tlog_play_exit_sighandler(int signum)
{
    if (tlog_play_exit_signum == 0) {
        tlog_play_exit_signum = signum;
    }
}

/* Number of IO signals caught */
static volatile sig_atomic_t tlog_play_io_caught;

static void
tlog_play_io_sighandler(int signum)
{
    (void)signum;
    tlog_play_io_caught++;
}

/**
 * Create an Elasticsearch JSON message reader according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param preader       Location for the created reader pointer. Not modified
 *                      in case of error.
 * @param conf          Configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_create_es_json_reader(struct tlog_errs **perrs,
                                struct tlog_json_reader **preader,
                                struct json_object *conf)
{
    tlog_grc grc;
    const char *baseurl;
    const char *query;
    struct tlog_json_reader *reader = NULL;
    struct json_object *obj;

    assert(preader != NULL);
    assert(conf != NULL);

    /* Get the base URL */
    if (!json_object_object_get_ex(conf, "baseurl", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Elasticsearch base URL is not specified");
    }
    baseurl = json_object_get_string(obj);

    /* Check base URL validity */
    if (!tlog_es_json_reader_base_url_is_valid(baseurl)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Invalid Elasticsearch base URL: %s", baseurl);
    }

    /* Get the query */
    if (!json_object_object_get_ex(conf, "query", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Elasticsearch query is not specified");
    }
    query = json_object_get_string(obj);

    /* Create the reader */
    grc = tlog_es_json_reader_create(&reader, baseurl, query, 10);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating the Elasticsearch reader");
    }

    *preader = reader;
    reader = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_reader_destroy(reader);
    return grc;
}

#ifdef TLOG_JOURNAL_ENABLED
/**
 * Create a journal JSON message reader according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param preader       Location for the created reader pointer. Not modified
 *                      in case of error.
 * @param conf          Configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_create_journal_json_reader(struct tlog_errs **perrs,
                                     struct tlog_json_reader **preader,
                                     struct json_object *conf)
{
    tlog_grc grc;
    const char **str_list = NULL;
    int64_t since = 0;
    int64_t until = INT64_MAX;
    size_t i;
    struct tlog_json_reader *reader = NULL;
    struct json_object *obj;

    assert(preader != NULL);
    assert(conf != NULL);

    /* Get the "since" timestamp */
    if (json_object_object_get_ex(conf, "since", &obj)) {
        since = json_object_get_int64(obj);
        if (since < 0) {
            since = 0;
        } else if ((uint64_t)since > UINT64_MAX/1000000) {
            since = UINT64_MAX/1000000;
        }
    } else {
        since = 0;
    }

    /* Get the "until" timestamp */
    if (json_object_object_get_ex(conf, "until", &obj)) {
        until = json_object_get_int64(obj);
        if (until < 0) {
            until = 0;
        } else if ((uint64_t)until > UINT64_MAX/1000000) {
            until = UINT64_MAX/1000000;
        }
    } else {
        until = INT64_MAX;
    }

    /* Get the match array, if any */

    if (json_object_object_get_ex(conf, "match", &obj)) {
        str_list = calloc(json_object_array_length(obj) + 1,
                          sizeof(*str_list));
        if (str_list == NULL) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed allocating systemd "
                              "journal match list");
        }
        for (i = 0; (int)i < (int)json_object_array_length(obj); i++) {
            str_list[i] = json_object_get_string(
                            json_object_array_get_idx(obj, i));
            if (!tlog_journal_match_sym_is_valid(str_list[i])) {
                grc = TLOG_RC_FAILURE;
                TLOG_ERRS_RAISEF("Systemd journal match symbol #%zu \"%s\" "
                                 "is invalid",
                                 i + 1, str_list[i]);
            }
        }
    }

    /* Create the reader */
    grc = tlog_journal_json_reader_create(&reader,
                                          (uint64_t)since * 1000000,
                                          (uint64_t)until * 1000000,
                                          str_list);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating the systemd journal reader");
    }

    *preader = reader;
    reader = NULL;
    grc = TLOG_RC_OK;

cleanup:
    free(str_list);
    tlog_json_reader_destroy(reader);
    return grc;
}
#endif

/**
 * Create a file JSON message reader according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param preader       Location for the created reader pointer. Not modified
 *                      in case of error.
 * @param conf          Configuration JSON object.
 *
 * @return Global return code.
*/
static tlog_grc
tlog_play_create_file_json_reader(struct tlog_errs **perrs,
                                  struct tlog_json_reader **preader,
                                  struct json_object *conf)
{
    tlog_grc grc;
    const char *str;
    int fd = -1;
    struct tlog_json_reader *reader = NULL;
    struct json_object *obj;

    assert(preader != NULL);
    assert(conf != NULL);

    /* Get the file path */
    if (!json_object_object_get_ex(conf, "path", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Log file path is not specified");
    }
    str = json_object_get_string(obj);

    /* Open the file */
    fd = open(str, O_RDONLY);
    if (fd < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECF(grc, "Failed opening log file \"%s\"", str);
    }

    /* Create the reader, letting it take over the FD */
    grc = tlog_fd_json_reader_create(&reader, fd, true, 65536);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISECS(grc, "Failed creating file reader");
    }
    fd = -1;

    *preader = reader;
    reader = NULL;
    grc = TLOG_RC_OK;

cleanup:

    if (fd >= 0) {
        close(fd);
    }
    tlog_json_reader_destroy(reader);
    return grc;
}

/**
 * Create a JSON message reader according to configuration.
 *
 * @param perrs         Location for the error stack. Can be NULL.
 * @param preader       Location for the created reader pointer. Not modified
 *                      in case of error.
 * @param conf          Configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_create_json_reader(struct tlog_errs **perrs,
                             struct tlog_json_reader **preader,
                             struct json_object *conf)
{
    tlog_grc grc;
    const char *str;
    struct json_object *obj;
    struct json_object *reader_conf;
    struct tlog_json_reader *reader = NULL;

    assert(preader != NULL);
    assert(conf != NULL);

    if (!json_object_object_get_ex(conf, "reader", &obj)) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISES("Reader type is not specified");
    }
    str = json_object_get_string(obj);
    if (strcmp(str, "es") == 0) {
        /* Get Elasticsearch reader conf container */
        if (!json_object_object_get_ex(conf, "es", &reader_conf)) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISES("Elasticsearch reader parameters "
                             "are not specified");
        }
        /* Create es reader */
        grc = tlog_play_create_es_json_reader(perrs, &reader, reader_conf);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
#ifdef TLOG_JOURNAL_ENABLED
    } else if (strcmp(str, "journal") == 0) {
        /* Get journal reader conf container */
        if (json_object_object_get_ex(conf, str, &reader_conf)) {
            /* Create journal reader */
            grc = tlog_play_create_journal_json_reader(perrs,
                                                       &reader,reader_conf);
            if (grc != TLOG_RC_OK) {
                goto cleanup;
            }
        } else {
            /* Create journal reader in the case conf = NULL*/
            grc = tlog_play_create_journal_json_reader(perrs, &reader, NULL);
            if (grc != TLOG_RC_OK) {
                goto cleanup;
            }
        }
#endif
    } else if (strcmp(str, "file") == 0) {
        /* Get file reader conf container */
        if (!json_object_object_get_ex(conf, str, &reader_conf)) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISES("File reader parameters are not specified");
        }
        /* Create file reader */
        grc = tlog_play_create_file_json_reader(perrs, &reader, reader_conf);
        if (grc != TLOG_RC_OK) {
            goto cleanup;
        }
    } else {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Unknown reader type: %s", str);
    }

    *preader = reader;
    reader = NULL;
    grc = TLOG_RC_OK;

cleanup:
    tlog_json_reader_destroy(reader);
    return grc;
}

/**
 * Create the log source according to configuration.
 *
 * @param perrs Location for the error stack. Can be NULL.
 * @param psink Location for the created source pointer.
 * @param conf  Configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_create_log_source(struct tlog_errs **perrs,
                            struct tlog_source **psource,
                            struct json_object *conf)
{
    tlog_grc grc;
    struct json_object *obj;
    struct tlog_json_reader *reader = NULL;
    struct tlog_source *source = NULL;

    /*
     * Create the reader
     */
    grc = tlog_play_create_json_reader(perrs, &reader, conf);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    /*
     * Create the source
     */
    {
        struct tlog_json_source_params params = {
            .reader = reader,
            .reader_owned = true,
            .io_size = 4096,
        };
        /* Get the "lax" flag */
        params.lax = json_object_object_get_ex(conf, "lax", &obj) &&
                     json_object_get_boolean(obj);
        /* Create the source */
        grc = tlog_json_source_create(&source, &params);
        if (grc != TLOG_RC_OK) {
            TLOG_ERRS_RAISECS(grc, "Failed creating the source");
        }
    }
    reader = NULL;

    *psource = source;
    source = NULL;
    grc = TLOG_RC_OK;

cleanup:

    tlog_json_reader_destroy(reader);
    tlog_source_destroy(source);
    return grc;
}

/** List of exit signals playback handles */
const int tlog_play_exit_sig_list[] = {SIGINT, SIGTERM, SIGHUP};

/** True, if we should poll after EOF */
bool tlog_play_follow = false;
/** Recorded data source */
struct tlog_source *tlog_play_source = NULL;
/** Current playback speed (time divisor) */
struct timespec tlog_play_speed = {1, 0};
/** True if "goto" function is active */
bool tlog_play_goto_active = false;
/** Timestamp the "goto" function should go to */
struct timespec tlog_play_goto_ts;
/** True if "skip" function is active */
bool tlog_play_skip = false;
/** True if playback is paused */
bool tlog_play_paused = false;
/** True if commands to quit playback should be ignored */
bool tlog_play_persist = false;
/** Original stdin file flags, -1 if not changed */
int tlog_play_stdin_flags = -1;
/* True if terminal attributes were changed */
bool tlog_play_term_attrs_set = false;
/** Original termios attributes */
struct termios tlog_play_orig_termios;
/** True if cURL global state was initialized, false otherwise */
bool tlog_play_curl_initialized = false;
/** True if have received at least one character of a timestamp from user */
bool tlog_play_got_ts = false;
/** Accumulated timestamp parser state, valid only if tlog_play_got_ts */
struct tlog_timestr_parser tlog_play_timestr_parser;
/** Local time of packet output last */
struct timespec tlog_play_local_last_ts;
/** Recording's time of packet output last */
struct timespec tlog_play_pkt_last_ts;

/** True if playback state was initialized succesfully */
bool tlog_play_initialized = false;

/**
 * Cleanup playback state. Can be called repeatedly.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_cleanup(struct tlog_errs **perrs)
{
    tlog_grc grc = TLOG_RC_OK;
    struct sigaction sa;
    ssize_t rc;
    size_t i;

    /* Destroy the source */
    tlog_source_destroy(tlog_play_source);
    tlog_play_source = NULL;

    /* Cleanup cURL */
    if (tlog_play_curl_initialized) {
        curl_global_cleanup();
        tlog_play_curl_initialized = false;
    }

    /* Restore stdin flags, if changed */
    if (tlog_play_stdin_flags >= 0) {
        if (fcntl(STDIN_FILENO, F_SETFL, tlog_play_stdin_flags) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed setting stdin flags");
        }
        tlog_play_stdin_flags = -1;
    }

    /* Restore signal handlers */
    for (i = 0; i < TLOG_ARRAY_SIZE(tlog_play_exit_sig_list); i++) {
        if (sigaction(tlog_play_exit_sig_list[i], NULL, &sa) == -1) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed to retrieve an exit signal action");
        }

        if (sa.sa_handler != SIG_IGN) {
            signal(tlog_play_exit_sig_list[i], SIG_DFL);
        }
    }
    signal(SIGIO, SIG_DFL);

    /* Restore terminal attributes */
    if (tlog_play_term_attrs_set) {
        rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &tlog_play_orig_termios);
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed restoring TTY attributes");
        }
        tlog_play_term_attrs_set = false;
    }

    /* Reset variables */
    tlog_play_follow = false;
    tlog_play_speed.tv_sec = 1;
    tlog_play_speed.tv_nsec = 0;
    tlog_play_goto_active = false;
    tlog_play_skip = false;
    tlog_play_paused = false;
    tlog_play_persist = false;
    tlog_play_got_ts = false;

    /* Mark playback state not initialized */
    tlog_play_initialized = false;

    return grc;
}

/**
 * Initialize playback state.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param conf      Configuration JSON object.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_init(struct tlog_errs **perrs,
               struct json_object *conf)
{
    tlog_grc grc;
    tlog_grc cleanup_grc;
    ssize_t rc;
    struct json_object *obj;
    const char *str;
    struct termios raw_termios;
    struct sigaction sa;
    size_t i;
    size_t j;

    assert(!tlog_play_initialized);

    /* Get the "speed" option, if any */
    if (json_object_object_get_ex(conf, "speed", &obj)) {
        tlog_timespec_from_fp(json_object_get_double(obj), &tlog_play_speed);
    }

    /* Get the "follow" flag */
    tlog_play_follow = json_object_object_get_ex(conf, "follow", &obj) &&
                    json_object_get_boolean(obj);

    /* Get the "paused" flag */
    tlog_play_paused = json_object_object_get_ex(conf, "paused", &obj) &&
                    json_object_get_boolean(obj);

    /* Get the "goto" location */
    if (json_object_object_get_ex(conf, "goto", &obj)) {
        str = json_object_get_string(obj);
        if (strcasecmp(str, "start") == 0) {
            tlog_play_goto_ts = tlog_timespec_zero;
        } else if (strcasecmp(str, "end") == 0) {
            tlog_play_goto_ts = tlog_timespec_max;
        } else if (!tlog_timestr_to_timespec(str, &tlog_play_goto_ts)) {
            grc = TLOG_RC_FAILURE;
            TLOG_ERRS_RAISEF("Failed parsing timestamp to go to: %s", str);
        }
        tlog_play_goto_active = true;
    }

    /* Get the "persist" flag */
    tlog_play_persist = json_object_object_get_ex(conf, "persist", &obj) &&
                     json_object_get_boolean(obj);

    /* Initialize libcurl */
    grc = TLOG_GRC_FROM(curl, curl_global_init(CURL_GLOBAL_NOTHING));
    if (grc != TLOG_GRC_FROM(curl, CURLE_OK)) {
        TLOG_ERRS_RAISECS(grc, "Failed Failed initializing libcurl");
    }
    tlog_play_curl_initialized = true;

    /* Create log source */
    grc = tlog_play_create_log_source(perrs, &tlog_play_source, conf);
    if (grc != TLOG_RC_OK) {
        TLOG_ERRS_RAISES("Failed creating log source");
    }

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(tlog_play_exit_sig_list); i++) {
        if (sigaction(tlog_play_exit_sig_list[i], NULL, &sa) == -1) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed to retrieve "
                              "an exit signal action");
        }
        if (sa.sa_handler != SIG_IGN) {
            sa.sa_handler = tlog_play_exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < TLOG_ARRAY_SIZE(tlog_play_exit_sig_list); j++) {
                sigaddset(&sa.sa_mask, tlog_play_exit_sig_list[j]);
            }
            /* NOTE: no SA_RESTART on purpose */
            sa.sa_flags = 0;
            if (sigaction(tlog_play_exit_sig_list[i], &sa, NULL) == -1) {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc, "Failed to set "
                                  "an exit signal action");
            }
        }
    }

    /* Setup IO signal handler */
    sa.sa_handler = tlog_play_io_sighandler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGIO);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    if (sigaction(SIGIO, &sa, NULL) == -1) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed to set SIGIO action");
    }

    /* Setup signal-driven IO on stdin (and stdout) */
    if (fcntl(STDIN_FILENO, F_SETOWN, getpid()) < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed taking over stdin signals");
    }
    tlog_play_stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (tlog_play_stdin_flags < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed getting stdin flags");
    }
    if (fcntl(STDIN_FILENO, F_SETFL,
              tlog_play_stdin_flags | O_ASYNC | O_NONBLOCK) < 0) {
        tlog_play_stdin_flags = -1;
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed setting stdin flags");
    }

    /* Get terminal attributes */
    rc = tcgetattr(STDOUT_FILENO, &tlog_play_orig_termios);
    if (rc < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed retrieving TTY attributes");
    }

    /*
     * Switch the terminal to raw mode,
     * but keep signal generation, if not persistent
     */
    raw_termios = tlog_play_orig_termios;
    raw_termios.c_lflag &= ~(ICANON | IEXTEN | ECHO |
                             (tlog_play_persist ? ISIG : 0));
    raw_termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                             INPCK | ISTRIP | IXON | PARMRK);
    raw_termios.c_oflag &= ~OPOST;
    raw_termios.c_cc[VMIN] = 1;
    raw_termios.c_cc[VTIME] = 0;
    rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw_termios);
    if (rc < 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed setting TTY attributes");
    }
    tlog_play_term_attrs_set = true;

    /*
     * Start the time
     */
    /* Set recording's last packet time to the start */
    tlog_play_pkt_last_ts = TLOG_TIMESPEC_ZERO;
    /* Set local last packet time to the current time */
    if (clock_gettime(CLOCK_MONOTONIC, &tlog_play_local_last_ts) != 0) {
        grc = TLOG_GRC_ERRNO;
        TLOG_ERRS_RAISECS(grc, "Failed retrieving current time");
    }

    tlog_play_initialized = true;
    grc = TLOG_RC_OK;

cleanup:

    if (grc != TLOG_RC_OK) {
        cleanup_grc = tlog_play_cleanup(perrs);
        if (cleanup_grc != TLOG_RC_OK) {
            grc = cleanup_grc;
        }
    }

    return grc;
}

/**
 * Read playback input, interpreting user input.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param pquit     Location for the "quit" flag. Set to true if playback was
 *                  signalled to quit. Set to false otherwise. Not modified on
 *                  error. Can be NULL.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_run_read_input(struct tlog_errs **perrs, bool *pquit)
{
    tlog_grc grc;
    ssize_t rc;
    /** True if playback should be stopped */
    bool quit = false;
    unsigned char c;
    struct timespec new_speed;
    const struct timespec max_speed = {16, 0};
    const struct timespec min_speed = {0, 62500000};
    const struct timespec accel = {2, 0};
    enum {
        /* Base state */
        STATE_BASE,
        /* Got ESC */
        STATE_ESC,
        /* Inside control sequence */
        STATE_CTL,
        /* Inside operating system command */
        STATE_OSC,
        /* Inside device control string */
        STATE_DCS,
        /* Inside mouse report sequence  */
        STATE_MOUSE,
        /* Number of states, not a valid state */
        STATE_NUM
    } state = STATE_BASE;
    /* Position in mouse report sequence */
    size_t mouse_seq_pos;

    while (!quit) {
        rc = read(STDIN_FILENO, &c, sizeof(c));
        if (rc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc, "Failed reading stdin");
            }
        } else if (rc == 0) {
            break;
        }

        /* Parse control sequences */
        /* FIXME Throw this away and embed a terminal emulator instead */
        if (state == STATE_BASE) {
            switch (c) {
            case '\x1b':
                state = STATE_ESC;
                continue;
            }
        } else if (state == STATE_ESC) {
            switch (c) {
            case '\x1b':
                state = STATE_ESC;
                continue;
            /* Control sequence */
            case '[':
            /* Cursor keys */
            case 'O':
                state = STATE_CTL;
                continue;
            /* OSC (Operating System Command) */
            case ']':
                state = STATE_OSC;
                continue;
            /* ST (String Terminator) */
            case '\\':
                state = STATE_BASE;
                continue;
            /* DCS (Device Control String) */
            case 'P':
                state = STATE_DCS;
                continue;
            default:
                state = STATE_BASE;
                /* If it's a parameter or an alphabetic */
                if (c >= 0x30 && c <= 0x7e) {
                    continue;
                }
                /* Proceed */
                break;
            }
        } else if (state == STATE_CTL) {
            /* If it's C0 control, an intermediate, a parameter, or Delete */
            if (c <= 0x3f || c == 0x7f) {
                continue;
            /* If it's a mouse report */
            } else if (c == 'M') {
                state = STATE_MOUSE;
                mouse_seq_pos = 0;
                continue;
            /* If it's an alphabetic */
            } else if (c >= 0x40 && c <= 0x7f) {
                state = STATE_BASE;
                continue;
            } else {
                state = STATE_BASE;
                /* Proceed */
            }
        } else if (state == STATE_OSC) {
            /* If it's the terminating BEL */
            if (c == '\x07') {
                state = STATE_BASE;
            /* If it's the possible beginning of ST */
            } else if (c == '\x1b') {
                state = STATE_ESC;
            }
            continue;
        } else if (state == STATE_DCS) {
            /* If it's the possible beginning of ST */
            if (c == '\x1b') {
                state = STATE_ESC;
            }
            continue;
        } else if (state == STATE_MOUSE) {
            mouse_seq_pos++;
            if (mouse_seq_pos >= 3) {
                state = STATE_BASE;
            }
            continue;
        }

        assert(state == STATE_BASE);

        /* Parse goto commands */
        switch (c) {
            case '0' ... '9':
            case ':':
                if (!tlog_play_got_ts) {
                    tlog_timestr_parser_reset(&tlog_play_timestr_parser);
                }
                tlog_play_got_ts = tlog_timestr_parser_feed(
                                        &tlog_play_timestr_parser, c);
                break;
            case 'G':
                if (tlog_play_got_ts) {
                    tlog_play_got_ts = false;
                    tlog_play_goto_active =
                        tlog_timestr_parser_yield(
                                            &tlog_play_timestr_parser,
                                            &tlog_play_goto_ts) &&
                        tlog_timespec_cmp(&tlog_play_goto_ts,
                                          &tlog_play_pkt_last_ts) > 0;
                } else {
                    tlog_play_goto_ts = tlog_timespec_max;
                    tlog_play_goto_active = true;
                }
                break;
            default:
                tlog_play_got_ts = false;
                break;
        }

        /* Parse other control keys */
        switch (c) {
            case ' ':
            case 'p':
                /* If unpausing */
                if (tlog_play_paused) {
                    /* Skip the time we were paused */
                    if (clock_gettime(CLOCK_MONOTONIC,
                                      &tlog_play_local_last_ts) != 0) {
                        grc = TLOG_GRC_ERRNO;
                        TLOG_ERRS_RAISECS(grc,
                                          "Failed retrieving current time");
                    }
                }
                tlog_play_paused = !tlog_play_paused;
                break;
            case '\x7f':
                tlog_play_speed = (struct timespec){1, 0};
                break;
            case '}':
                tlog_timespec_fp_mul(&tlog_play_speed, &accel,
                                     &new_speed);
                if (tlog_timespec_cmp(&new_speed, &max_speed) <= 0) {
                    tlog_play_speed = new_speed;
                }
                break;
            case '{':
                tlog_timespec_fp_div(&tlog_play_speed, &accel,
                                     &new_speed);
                if (tlog_timespec_cmp(&new_speed, &min_speed) >= 0) {
                    tlog_play_speed = new_speed;
                }
                break;
            case '.':
                tlog_play_skip = true;
                break;
            case 'q':
                quit = !tlog_play_persist;
                break;
            default:
                break;
        }
    }

    if (pquit != NULL) {
        *pquit = quit;
    }

    grc = TLOG_RC_OK;
cleanup:
    return grc;
}

/**
 * Run playback with the initialized state.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param psignal   Location for the number of the signal which caused
 *                  playback to exit, zero if none. Not modified in case of
 *                  error. Can be NULL.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_play_run(struct tlog_errs **perrs, int *psignal)
{
    tlog_grc grc;
    ssize_t rc;
    /** Local time now */
    struct timespec local_this_ts;
    /** Local time of packet output next */
    struct timespec local_next_ts;
    /** Delay to the packet output next */
    struct timespec pkt_delay_ts;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    struct tlog_pkt_pos pos = TLOG_PKT_POS_VOID;
    size_t loc_num;
    char *loc_str = NULL;
    struct timespec read_wait = TLOG_TIMESPEC_ZERO;
    sig_atomic_t last_io_caught = 0;
    sig_atomic_t new_io_caught;
    struct pollfd std_pollfds[2] = {[STDIN_FILENO] = {.fd = STDIN_FILENO,
                                                      .events = POLLIN},
                                    [STDOUT_FILENO] = {.fd = STDOUT_FILENO,
                                                       .events = POLLOUT}};

    assert(tlog_play_initialized);

    tlog_play_exit_signum = 0;
    tlog_play_io_caught = 0;

    /*
     * Reproduce the logged output
     */
    while (tlog_play_exit_signum == 0) {
        /* React to input */
        new_io_caught = tlog_play_io_caught;
        if (new_io_caught != last_io_caught) {
            bool quit = false;
            grc = tlog_play_run_read_input(perrs, &quit);
            if (grc != TLOG_RC_OK) {
                goto cleanup;
            }
            last_io_caught = new_io_caught;
            if (quit) {
                break;
            }
        }

        /* Handle pausing, unless ignoring timing */
        if (tlog_play_paused && !(tlog_play_goto_active || tlog_play_skip)) {
            do {
                rc = clock_nanosleep(CLOCK_MONOTONIC, 0,
                                     &tlog_timespec_max, NULL);
            } while (rc == 0);
            if (rc == EINTR) {
                continue;
            } else {
                grc = TLOG_GRC_FROM(errno, rc);
                TLOG_ERRS_RAISECS(grc, "Failed sleeping");
            }
        }

        /* If there is no data in the packet */
        if (tlog_pkt_is_void(&pkt)) {
            /* Wait for next read, if necessary */
            if (!tlog_timespec_is_zero(&read_wait)) {
                rc = clock_nanosleep(CLOCK_MONOTONIC, 0,
                                     &read_wait, &read_wait);
                if (rc == 0) {
                    read_wait = TLOG_TIMESPEC_ZERO;
                } if (rc == EINTR) {
                    continue;
                } else if (rc != 0) {
                    grc = TLOG_GRC_FROM(errno, rc);
                    TLOG_ERRS_RAISECS(grc, "Failed sleeping");
                }
            }
            /* Read a packet */
            loc_num = tlog_source_loc_get(tlog_play_source);
            grc = tlog_source_read(tlog_play_source, &pkt);
            if (grc == TLOG_GRC_FROM(errno, EINTR)) {
                continue;
            } else if (grc != TLOG_RC_OK) {
                tlog_errs_pushc(perrs, grc);
                loc_str = tlog_source_loc_fmt(tlog_play_source, loc_num);
                TLOG_ERRS_RAISEF("Failed reading the source at %s", loc_str);
            }
            /* If hit the end of stream */
            if (tlog_pkt_is_void(&pkt)) {
                tlog_play_goto_active = false;
                if (tlog_play_follow) {
                    read_wait = (struct timespec){POLL_PERIOD, 0};
                    continue;
                } else if (tlog_play_paused) {
                    continue;
                } else {
                    break;
                }
            }
            /* If it's not the output */
            if (pkt.type != TLOG_PKT_TYPE_IO || !pkt.data.io.output) {
                tlog_pkt_cleanup(&pkt);
                continue;
            }
        }

        /* Get current time */
        if (clock_gettime(CLOCK_MONOTONIC, &local_this_ts) != 0) {
            grc = TLOG_GRC_ERRNO;
            TLOG_ERRS_RAISECS(grc, "Failed retrieving current time");
        }

        /* If we're skipping the timing of this packet */
        if (tlog_play_skip) {
            /* Skip the time */
            tlog_play_local_last_ts = local_this_ts;
            tlog_play_skip = false;
        /* Else, if we're fast-forwarding to a time */
        } else if (tlog_play_goto_active) {
            /* Skip the time */
            tlog_play_local_last_ts = local_this_ts;
            /* If we reached the target time */
            if (tlog_timespec_cmp(&pkt.timestamp, &tlog_play_goto_ts) >= 0) {
                tlog_play_goto_active = false;
                tlog_play_pkt_last_ts = tlog_play_goto_ts;
                continue;
            }
        } else {
            tlog_timespec_sub(&pkt.timestamp, &tlog_play_pkt_last_ts, &pkt_delay_ts);
            tlog_timespec_fp_div(&pkt_delay_ts, &tlog_play_speed, &pkt_delay_ts);
            tlog_timespec_cap_add(&tlog_play_local_last_ts, &pkt_delay_ts,
                                  &local_next_ts);
            /* If we don't need a delay for the next packet (it's overdue) */
            if (tlog_timespec_cmp(&local_next_ts, &local_this_ts) <= 0) {
                /* Stretch the time */
                tlog_play_local_last_ts = local_this_ts;
            } else {
                /* Advance the time */
                rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                     &local_next_ts, NULL);
                /* If we're interrupted */
                if (rc == EINTR) {
                    tlog_play_local_last_ts = local_this_ts;
                    /* Get current time */
                    if (clock_gettime(CLOCK_MONOTONIC, &local_this_ts) != 0) {
                        grc = TLOG_GRC_ERRNO;
                        TLOG_ERRS_RAISECS(grc,
                                          "Failed retrieving current time");
                    }
                    /* Note the interruption time */
                    tlog_timespec_sub(&local_this_ts, &tlog_play_local_last_ts,
                                      &pkt_delay_ts);
                    tlog_timespec_fp_mul(&pkt_delay_ts, &tlog_play_speed,
                                         &pkt_delay_ts);
                    tlog_timespec_cap_add(&tlog_play_pkt_last_ts, &pkt_delay_ts,
                                          &tlog_play_pkt_last_ts);
                    tlog_play_local_last_ts = local_this_ts;
                    continue;
                } else if (rc != 0) {
                    grc = TLOG_GRC_FROM(errno, rc);
                    TLOG_ERRS_RAISECS(grc, "Failed sleeping");
                }
                tlog_play_local_last_ts = local_next_ts;
            }
        }

        /*
         * Output the packet emulating synchronous I/O.
         * Otherwise we would get EAGAIN/EWOULDBLOCK.
         */
        /* Wait for I/O on the terminal */
        do {
            rc = ppoll(std_pollfds, TLOG_ARRAY_SIZE(std_pollfds), NULL, NULL);
        } while (rc == 0);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc, "Failed waiting for terminal I/O");
            }
        } else if (std_pollfds[STDIN_FILENO].revents != 0) {
            /* Prioritize input handling */
            continue;
        }
        /* Output the packet */
        rc = write(STDOUT_FILENO,
                   pkt.data.io.buf + pos.val, pkt.data.io.len - pos.val);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                grc = TLOG_GRC_ERRNO;
                TLOG_ERRS_RAISECS(grc, "Failed writing output");
            }
        }
        tlog_play_pkt_last_ts = pkt.timestamp;
        /* Consume the output part (or the whole) of the packet */
        tlog_pkt_pos_move(&pos, &pkt, rc);
        if (tlog_pkt_pos_is_past(&pos, &pkt)) {
            pos = TLOG_PKT_POS_VOID;
            tlog_pkt_cleanup(&pkt);
        }
    }

    if (psignal != NULL) {
        *psignal = tlog_play_exit_signum;
    }
    grc = TLOG_RC_OK;

cleanup:
    free(loc_str);
    tlog_pkt_cleanup(&pkt);

    return grc;
}

tlog_grc
tlog_play(struct tlog_errs **perrs,
          const char *cmd_help,
          struct json_object *conf,
          int *psignal)
{
    tlog_grc grc;
    tlog_grc cleanup_grc;
    struct json_object *obj;
    int signal = 0;

    /* Check if arguments are provided */
    if (json_object_object_get_ex(conf, "args", &obj) &&
        json_object_array_length(obj) > 0) {
        grc = TLOG_RC_FAILURE;
        TLOG_ERRS_RAISEF("Positional arguments are not accepted\n%s",
                        cmd_help);
    }

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
            printf("%s", tlog_version);
            grc = TLOG_RC_OK;
            goto cleanup;
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

    /* Output and discard any accumulated non-critical error messages */
    tlog_errs_print(stderr, *perrs);
    tlog_errs_destroy(perrs);

    /* Initialize playback state */
    grc = tlog_play_init(perrs, conf);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    /* Run playback */
    grc = tlog_play_run(perrs, &signal);

cleanup:

    /* Cleanup playback state */
    cleanup_grc = tlog_play_cleanup(perrs);
    if (cleanup_grc != TLOG_RC_OK) {
        grc = cleanup_grc;
    }

    /* Clear off remaining reproduced output */
    {
        const char newline = '\n';
        ssize_t rc;

        rc = write(STDOUT_FILENO, &newline, sizeof(newline));
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed writing newline to TTY");
        }
    }

    if (grc == TLOG_RC_OK) {
        *psignal = signal;
    }

    return grc;
}
