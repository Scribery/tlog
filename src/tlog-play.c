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
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <curl/curl.h>
#include <locale.h>
#include <langinfo.h>
#include <tlog/play_conf.h>
#include <tlog/play_conf_cmd.h>
#include <tlog/fd_json_reader.h>
#include <tlog/es_json_reader.h>
#ifdef TLOG_JOURNAL_ENABLED
#include <tlog/journal_json_reader.h>
#include <tlog/journal_misc.h>
#endif
#include <tlog/json_source.h>
#include <tlog/rc.h>
#include <tlog/timespec.h>
#include <tlog/timestr.h>
#include <tlog/misc.h>

#define POLL_PERIOD 1

/**< Number of the signal causing exit */
static volatile sig_atomic_t exit_signum  = 0;

static void
exit_sighandler(int signum)
{
    if (exit_signum == 0) {
        exit_signum = signum;
    }
}

/* Number of IO signals caught */
static volatile sig_atomic_t io_caught;

static void
io_sighandler(int signum)
{
    (void)signum;
    io_caught++;
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
create_log_source(struct tlog_errs **perrs,
                  struct tlog_source **psource,
                  struct json_object *conf)
{
    tlog_grc grc;
    struct json_object *obj;
    const char *str;
    int fd = -1;
    size_t i;
    const char **str_list = NULL;
    struct tlog_json_reader *reader = NULL;
    struct tlog_source *source = NULL;

    /*
     * Create the reader
     */
    if (!json_object_object_get_ex(conf, "reader", &obj)) {
        tlog_errs_pushs(perrs, "Reader type is not specified");
        grc = TLOG_RC_FAILURE;
        goto cleanup;
    }
    str = json_object_get_string(obj);
    if (strcmp(str, "es") == 0) {
        struct json_object *conf_es;
        const char *baseurl;
        const char *query;

        /* Get Elasticsearch reader conf container */
        if (!json_object_object_get_ex(conf, "es", &conf_es)) {
            tlog_errs_pushs(perrs,
                            "Elasticsearch reader parameters "
                            "are not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get the base URL */
        if (!json_object_object_get_ex(conf_es, "baseurl", &obj)) {
            tlog_errs_pushs(perrs, "Elasticsearch base URL is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        baseurl = json_object_get_string(obj);

        /* Check base URL validity */
        if (!tlog_es_json_reader_base_url_is_valid(baseurl)) {
            tlog_errs_pushf(perrs,
                            "Invalid Elasticsearch base URL: %s", baseurl);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get the query */
        if (!json_object_object_get_ex(conf_es, "query", &obj)) {
            tlog_errs_pushs(perrs, "Elasticsearch query is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        query = json_object_get_string(obj);

        /* Create the reader */
        grc = tlog_es_json_reader_create(&reader, baseurl, query, 10);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating the Elasticsearch reader");
            goto cleanup;
        }
#ifdef TLOG_JOURNAL_ENABLED
    } else if (strcmp(str, "journal") == 0) {
        struct json_object *conf_journal;
        int64_t since = 0;
        int64_t until = INT64_MAX;

        /* Get journal reader conf container */
        if (json_object_object_get_ex(conf, "journal", &conf_journal)) {
            /* Get the "since" timestamp */
            if (json_object_object_get_ex(conf_journal, "since", &obj)) {
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
            if (json_object_object_get_ex(conf_journal, "until", &obj)) {
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
            if (json_object_object_get_ex(conf_journal, "match", &obj)) {
                str_list = calloc(json_object_array_length(obj) + 1,
                                  sizeof(*str_list));
                if (str_list == NULL) {
                    grc = TLOG_GRC_ERRNO;
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs,
                                    "Failed allocating systemd journal match list");
                    goto cleanup;
                }
                for (i = 0; (int)i < (int)json_object_array_length(obj); i++) {
                    str_list[i] = json_object_get_string(
                                    json_object_array_get_idx(obj, i));
                    if (!tlog_journal_match_sym_is_valid(str_list[i])) {
                        grc = TLOG_RC_FAILURE;
                        tlog_errs_pushf(
                            perrs,
                            "Systemd journal match symbol #%zu \"%s\" "
                            "is invalid",
                            i + 1, str_list[i]);
                        goto cleanup;
                    }
                }
            }
        }

        /* Create the reader */
        grc = tlog_journal_json_reader_create(&reader,
                                              (uint64_t)since * 1000000,
                                              (uint64_t)until * 1000000,
                                              str_list);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs,
                            "Failed creating the systemd journal reader");
            goto cleanup;
        }

#endif
    } else if (strcmp(str, "file") == 0) {
        struct json_object *conf_file;

        /* Get file reader conf container */
        if (!json_object_object_get_ex(conf, "file", &conf_file)) {
            tlog_errs_pushs(perrs,
                            "File reader parameters are not specified");
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
        fd = open(str, O_RDONLY);
        if (fd < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushf(perrs, "Failed opening log file \"%s\"", str);
            goto cleanup;
        }

        /* Create the reader, letting it take over the FD */
        grc = tlog_fd_json_reader_create(&reader, fd, true, 65536);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating file reader");
            goto cleanup;
        }
        fd = -1;
    } else {
        tlog_errs_pushf(perrs, "Unknown reader type: %s", str);
        grc = TLOG_RC_FAILURE;
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
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating the source");
            goto cleanup;
        }
    }
    reader = NULL;

    *psource = source;
    source = NULL;
    grc = TLOG_RC_OK;

cleanup:

    if (fd >= 0) {
        close(fd);
    }
    free(str_list);
    tlog_json_reader_destroy(reader);
    tlog_source_destroy(source);
    return grc;
}

static tlog_grc
run(struct tlog_errs **perrs,
    const char *cmd_help,
    struct json_object *conf)
{
    const int exit_sig[] = {SIGINT, SIGTERM, SIGHUP};
    tlog_grc grc;
    struct json_object *obj;
    /* True, if we should poll after EOF */
    bool follow;
    /* Local time of packet output last */
    struct timespec local_last_ts;
    /* Local time now */
    struct timespec local_this_ts;
    /* Local time of packet output next */
    struct timespec local_next_ts;
    /* Recording time of the packet output last */
    struct timespec pkt_last_ts = TLOG_TIMESPEC_ZERO;
    /* Delay to the packet output next */
    struct timespec pkt_delay_ts;
    ssize_t rc;
    size_t i;
    size_t j;
    const char *str;
    bool term_attrs_set = false;
    struct termios orig_termios;
    struct termios raw_termios;
    struct sigaction sa;
    int stdin_flags = -1;
    struct tlog_source *source = NULL;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    struct tlog_pkt_pos pos = TLOG_PKT_POS_VOID;
    size_t loc_num;
    char *loc_str = NULL;
    struct timespec read_wait = TLOG_TIMESPEC_ZERO;
    sig_atomic_t last_io_caught = 0;
    sig_atomic_t new_io_caught;
    char key;
    struct pollfd std_pollfds[2] = {[STDIN_FILENO] = {.fd = STDIN_FILENO,
                                                      .events = POLLIN},
                                    [STDOUT_FILENO] = {.fd = STDOUT_FILENO,
                                                       .events = POLLOUT}};
    struct timespec speed = {1, 0};
    struct timespec new_speed;
    const struct timespec max_speed = {16, 0};
    const struct timespec min_speed = {0, 62500000};
    const struct timespec accel = {2, 0};
    bool got_ts = false;
    struct tlog_timestr_parser timestr_parser;
    bool goto_active = false;
    struct timespec goto_ts;
    bool paused = false;
    bool quit = false;
    bool persist = false;
    bool skip = false;

    assert(cmd_help != NULL);

    io_caught = 0;

    /* Check if arguments are provided */
    if (json_object_object_get_ex(conf, "args", &obj) &&
        json_object_array_length(obj) > 0) {
        tlog_errs_pushf(perrs,
                        "Positional arguments are not accepted\n%s",
                        cmd_help);
        grc = TLOG_RC_FAILURE;
        goto cleanup;
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
            printf("%s", tlog_version);;
            grc = TLOG_RC_OK;
            goto cleanup;
        }
    }

    /* Get the "speed" option */
    if (json_object_object_get_ex(conf, "speed", &obj)) {
        tlog_timespec_from_fp(json_object_get_double(obj), &speed);
    }

    /* Get the "follow" flag */
    follow = json_object_object_get_ex(conf, "follow", &obj) &&
             json_object_get_boolean(obj);

    /* Get the "paused" flag */
    paused = json_object_object_get_ex(conf, "paused", &obj) &&
             json_object_get_boolean(obj);

    /* Get the "goto" location */
    if (json_object_object_get_ex(conf, "goto", &obj)) {
        str = json_object_get_string(obj);
        if (strcasecmp(str, "start") == 0) {
            goto_ts = tlog_timespec_zero;
        } else if (strcasecmp(str, "end") == 0) {
            goto_ts = tlog_timespec_max;
        } else if (!tlog_timestr_to_timespec(str, &goto_ts)) {
            tlog_errs_pushf(perrs,
                            "Failed parsing timestamp to go to: %s", str);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        goto_active = true;
    }

    /* Get the "persist" flag */
    persist = json_object_object_get_ex(conf, "persist", &obj) &&
              json_object_get_boolean(obj);

    /* Initialize libcurl */
    grc = TLOG_GRC_FROM(curl, curl_global_init(CURL_GLOBAL_NOTHING));
    if (grc != TLOG_GRC_FROM(curl, CURLE_OK)) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed initializing libcurl");
        goto cleanup;
    }

    /* Create log source */
    grc = create_log_source(perrs, &source, conf);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushs(perrs, "Failed creating log source");
        goto cleanup;
    }

    /* Get terminal attributes */
    rc = tcgetattr(STDOUT_FILENO, &orig_termios);
    if (rc < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving TTY attributes");
        goto cleanup;
    }

    /* Output and discard any accumulated non-critical error messages */
    tlog_errs_print(stderr, *perrs);
    tlog_errs_destroy(perrs);

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        if(sigaction(exit_sig[i], NULL, &sa) == -1){
          grc = TLOG_GRC_ERRNO;
          tlog_errs_pushc(perrs, grc);
          tlog_errs_pushs(perrs, "Failed to retrieve an exit signal action");
          goto cleanup;
        }
        if (sa.sa_handler != SIG_IGN) {
            sa.sa_handler = exit_sighandler;
            sigemptyset(&sa.sa_mask);
            for (j = 0; j < TLOG_ARRAY_SIZE(exit_sig); j++) {
                sigaddset(&sa.sa_mask, exit_sig[j]);
            }
            /* NOTE: no SA_RESTART on purpose */
            sa.sa_flags = 0;
            if(sigaction(exit_sig[i], &sa, NULL) == -1){
              grc = TLOG_GRC_ERRNO;
              tlog_errs_pushc(perrs, grc);
              tlog_errs_pushs(perrs, "Failed to set an exit signal action");
              goto cleanup;
            }
        }
    }

    /* Setup IO signal handler */
    sa.sa_handler = io_sighandler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGIO);
    /* NOTE: no SA_RESTART on purpose */
    sa.sa_flags = 0;
    if(sigaction(SIGIO, &sa, NULL) == -1){
      grc = TLOG_GRC_ERRNO;
      tlog_errs_pushc(perrs, grc);
      tlog_errs_pushs(perrs, "Failed to set SIGIO action");
      goto cleanup;
    }

    /* Setup signal-driven IO on stdin (and stdout) */
    if (fcntl(STDIN_FILENO, F_SETOWN, getpid()) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed taking over stdin signals");
        goto cleanup;
    }
    stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (stdin_flags < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed getting stdin flags");
        goto cleanup;
    }
    if (fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_ASYNC | O_NONBLOCK) < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed setting stdin flags");
        goto cleanup;
    }

    /*
     * Switch the terminal to raw mode,
     * but keep signal generation, if not persistent
     */
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | IEXTEN | ECHO | (persist ? ISIG : 0));
    raw_termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                             INPCK | ISTRIP | IXON | PARMRK);
    raw_termios.c_oflag &= ~OPOST;
    raw_termios.c_cc[VMIN] = 1;
    raw_termios.c_cc[VTIME] = 0;
    rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw_termios);
    if (rc < 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed setting TTY attributes");
        goto cleanup;
    }
    term_attrs_set = true;

    /*
     * Start the time
     */
    /* Get current time */
    if (clock_gettime(CLOCK_MONOTONIC, &local_last_ts) != 0) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed retrieving current time");
        goto cleanup;
    }

    /*
     * Reproduce the logged output
     */
    while (exit_signum == 0) {
        /* React to input */
        new_io_caught = io_caught;
        if (new_io_caught != last_io_caught) {
            while (!quit) {
                rc = read(STDIN_FILENO, &key, sizeof(key));
                if (rc < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        last_io_caught = new_io_caught;
                        break;
                    } else {
                        grc = TLOG_GRC_ERRNO;
                        tlog_errs_pushc(perrs, grc);
                        tlog_errs_pushs(perrs, "Failed reading stdin");
                        goto cleanup;
                    }
                } else if (rc == 0) {
                    last_io_caught = new_io_caught;
                    break;
                } else {
                    switch (key) {
                        case '0' ... '9':
                        case ':':
                            if (!got_ts) {
                                tlog_timestr_parser_reset(&timestr_parser);
                            }
                            got_ts = tlog_timestr_parser_feed(&timestr_parser, key);
                            break;
                        case 'G':
                            if (got_ts) {
                                got_ts = false;
                                goto_active =
                                    tlog_timestr_parser_yield(&timestr_parser,
                                                              &goto_ts);
                            } else {
                                goto_ts = tlog_timespec_max;
                                goto_active = true;
                            }
                            break;
                        default:
                            got_ts = false;
                            break;
                    }
                    switch (key) {
                        case ' ':
                        case 'p':
                            /* If unpausing */
                            if (paused) {
                                /* Skip the time we were paused */
                                if (clock_gettime(CLOCK_MONOTONIC,
                                                  &local_last_ts) != 0) {
                                    grc = TLOG_GRC_ERRNO;
                                    tlog_errs_pushc(perrs, grc);
                                    tlog_errs_pushs(
                                            perrs,
                                            "Failed retrieving current time");
                                    goto cleanup;
                                }
                            }
                            paused = !paused;
                            break;
                        case '\x7f':
                            speed = (struct timespec){1, 0};
                            break;
                        case '}':
                            tlog_timespec_fp_mul(&speed, &accel, &new_speed);
                            if (tlog_timespec_cmp(&new_speed, &max_speed) <= 0) {
                                speed = new_speed;
                            }
                            break;
                        case '{':
                            tlog_timespec_fp_div(&speed, &accel, &new_speed);
                            if (tlog_timespec_cmp(&new_speed, &min_speed) >= 0) {
                                speed = new_speed;
                            }
                            break;
                        case '.':
                            skip = true;
                            break;
                        case 'q':
                            quit = !persist;
                            break;
                        default:
                            break;
                    }
                }
            }
            if (quit) {
                break;
            }
        }

        /* Handle pausing, unless ignoring timing */
        if (paused && !(goto_active || skip)) {
            do {
                rc = clock_nanosleep(CLOCK_MONOTONIC, 0,
                                     &tlog_timespec_max, NULL);
            } while (rc == 0);
            if (rc == EINTR) {
                continue;
            } else {
                grc = TLOG_GRC_FROM(errno, rc);
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed sleeping");
                goto cleanup;
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
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs, "Failed sleeping");
                    goto cleanup;
                }
            }
            /* Read a packet */
            loc_num = tlog_source_loc_get(source);
            grc = tlog_source_read(source, &pkt);
            if (grc == TLOG_GRC_FROM(errno, EINTR)) {
                continue;
            } else if (grc != TLOG_RC_OK) {
                tlog_errs_pushc(perrs, grc);
                loc_str = tlog_source_loc_fmt(source, loc_num);
                tlog_errs_pushf(perrs, "Failed reading the source at %s", loc_str);
                goto cleanup;
            }
            /* If hit the end of stream */
            if (tlog_pkt_is_void(&pkt)) {
                goto_active = false;
                if (follow) {
                    read_wait = (struct timespec){POLL_PERIOD, 0};
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
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed retrieving current time");
            goto cleanup;
        }

        /* If we're skipping the timing of this packet */
        if (skip) {
            /* Skip the time */
            local_last_ts = local_this_ts;
            skip = false;
        /* Else, if we're fast-forwarding to a time */
        } else if (goto_active) {
            /* Skip the time */
            local_last_ts = local_this_ts;
            /* If we reached the target time */
            if (tlog_timespec_cmp(&pkt.timestamp, &goto_ts) >= 0) {
                goto_active = false;
                pkt_last_ts = goto_ts;
                continue;
            }
        } else {
            tlog_timespec_sub(&pkt.timestamp, &pkt_last_ts, &pkt_delay_ts);
            tlog_timespec_fp_div(&pkt_delay_ts, &speed, &pkt_delay_ts);
            tlog_timespec_cap_add(&local_last_ts, &pkt_delay_ts, &local_next_ts);
            /* If we don't need a delay for the next packet (it's overdue) */
            if (tlog_timespec_cmp(&local_next_ts, &local_this_ts) <= 0) {
                /* Stretch the time */
                local_last_ts = local_this_ts;
            } else {
                /* Advance the time */
                rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                     &local_next_ts, NULL);
                /* If we're interrupted */
                if (rc == EINTR) {
                    local_last_ts = local_this_ts;
                    /* Get current time */
                    if (clock_gettime(CLOCK_MONOTONIC, &local_this_ts) != 0) {
                        grc = TLOG_GRC_ERRNO;
                        tlog_errs_pushc(perrs, grc);
                        tlog_errs_pushs(perrs,
                                        "Failed retrieving current time");
                        goto cleanup;
                    }
                    /* Note the interruption time */
                    tlog_timespec_sub(&local_this_ts, &local_last_ts,
                                      &pkt_delay_ts);
                    tlog_timespec_fp_mul(&pkt_delay_ts, &speed, &pkt_delay_ts);
                    tlog_timespec_cap_add(&pkt_last_ts, &pkt_delay_ts,
                                          &pkt_last_ts);
                    local_last_ts = local_this_ts;
                    continue;
                } else if (rc != 0) {
                    grc = TLOG_GRC_FROM(errno, rc);
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs, "Failed sleeping");
                    goto cleanup;
                }
                local_last_ts = local_next_ts;
            }
        }

        /*
         * Output the packet emulating synchronous I/O.
         * Otherwise we would get EAGAIN/EWOULDBLOCK.
         */
        /* Wait for I/O on the terminal */
        do {
            rc = ppoll(std_pollfds, TLOG_ARRAY_SIZE(std_pollfds),
                       &tlog_timespec_max, NULL);
        } while (rc == 0);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                grc = TLOG_GRC_ERRNO;
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed waiting for terminal I/O");
                goto cleanup;
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
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed writing output");
                goto cleanup;
            }
        }
        pkt_last_ts = pkt.timestamp;
        /* Consume the output part (or the whole) of the packet */
        tlog_pkt_pos_move(&pos, &pkt, rc);
        if (tlog_pkt_pos_is_past(&pos, &pkt)) {
            pos = TLOG_PKT_POS_VOID;
            tlog_pkt_cleanup(&pkt);
        }
    }

    grc = TLOG_RC_OK;

cleanup:

    free(loc_str);
    tlog_pkt_cleanup(&pkt);
    tlog_source_destroy(source);
    curl_global_cleanup();

    /* Restore stdin flags, if changed */
    if (stdin_flags >= 0) {
        if (fcntl(STDIN_FILENO, F_SETFL, stdin_flags) < 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed setting stdin flags");
        }
    }

    /* Restore signal handlers */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        if(sigaction(exit_sig[i], NULL, &sa) == -1) {
          grc = TLOG_GRC_ERRNO;
          tlog_errs_pushc(perrs, grc);
          tlog_errs_pushs(perrs, "Failed to retrieve an exit signal action");
        }

        if (sa.sa_handler != SIG_IGN) {
            signal(exit_sig[i], SIG_DFL);
        }
    }
    signal(SIGIO, SIG_DFL);

    /* Restore terminal attributes */
    if (term_attrs_set) {
        const char newline = '\n';

        rc = tcsetattr(STDOUT_FILENO, TCSAFLUSH, &orig_termios);
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed restoring TTY attributes");
        }

        /* Clear off remaining reproduced output */
        rc = write(STDOUT_FILENO, &newline, sizeof(newline));
        if (rc < 0 && errno != EBADF) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed writing newline to TTY");
        }
    }

    return grc;
}

int
main(int argc, char **argv)
{
    tlog_grc grc;
    struct tlog_errs *errs = NULL;
    struct json_object *conf = NULL;
    char *cmd_help = NULL;
    const char *charset;

    /* Set locale from environment variables */
    if (setlocale(LC_ALL, "") == NULL) {
        grc = TLOG_GRC_ERRNO;
        tlog_errs_pushc(&errs, grc);
        tlog_errs_pushs(&errs,
                        "Failed setting locale from environment variables");
        goto cleanup;
    }

    /* Read configuration and command-line usage message */
    grc = tlog_play_conf_load(&errs, &cmd_help, &conf, argc, argv);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    /* Check that the character encoding is supported */
    charset = nl_langinfo(CODESET);
    if (strcmp(charset, "UTF-8") != 0) {
        if (strcmp(charset, "ANSI_X3.4-1968") == 0) {
            tlog_errs_pushf(&errs, "Locale charset is ANSI_X3.4-1968 (ASCII)");
            tlog_errs_pushf(&errs, "Assuming locale environment is lost "
                                   "and charset is UTF-8");
        } else {
            grc = TLOG_RC_FAILURE;
            tlog_errs_pushf(&errs, "Unsupported locale charset: %s", charset);
            goto cleanup;
        }
    }

    /* Run */
    grc = run(&errs, cmd_help, conf);

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

    return grc != TLOG_RC_OK;
}
