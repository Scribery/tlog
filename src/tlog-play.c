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
#include <curl/curl.h>
#include <locale.h>
#include <langinfo.h>
#include <tlog/play_conf.h>
#include <tlog/play_conf_cmd.h>
#include <tlog/fd_json_reader.h>
#include <tlog/es_json_reader.h>
#include <tlog/json_source.h>
#include <tlog/rc.h>
#include <tlog/timespec.h>
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

        /* Get ElasticSearch reader conf container */
        if (!json_object_object_get_ex(conf, "es", &conf_es)) {
            tlog_errs_pushs(perrs,
                            "ElasticSearch reader parameters "
                            "are not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get the base URL */
        if (!json_object_object_get_ex(conf_es, "baseurl", &obj)) {
            tlog_errs_pushs(perrs, "ElasticSearch base URL is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        baseurl = json_object_get_string(obj);

        /* Check base URL validity */
        if (!tlog_es_json_reader_base_url_is_valid(baseurl)) {
            tlog_errs_pushf(perrs,
                            "Invalid ElasticSearch base URL: %s", baseurl);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        /* Get the query */
        if (!json_object_object_get_ex(conf_es, "query", &obj)) {
            tlog_errs_pushs(perrs, "ElasticSearch query is not specified");
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        query = json_object_get_string(obj);

        /* Create the reader */
        grc = tlog_es_json_reader_create(&reader, baseurl, query, 10);
        if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed creating the ElasticSearch reader");
            goto cleanup;
        }
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

    /* Create the source */
    grc = tlog_json_source_create(&source, reader, true,
                                  NULL, NULL, NULL, 0, 4096);
    if (grc != TLOG_RC_OK) {
        tlog_errs_pushc(perrs, grc);
        tlog_errs_pushs(perrs, "Failed creating the source");
        goto cleanup;
    }
    reader = NULL;

    *psource = source;
    source = NULL;
    grc = TLOG_RC_OK;

cleanup:

    if (fd >= 0) {
        close(fd);
    }
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
    bool follow;
    struct timespec local_last_ts;
    struct timespec local_this_ts;
    struct timespec local_next_ts;
    struct timespec pkt_last_ts;
    struct timespec pkt_delay_ts;
    ssize_t rc;
    size_t i;
    size_t j;
    bool term_attrs_set = false;
    struct termios orig_termios;
    struct termios raw_termios;
    struct sigaction sa;
    struct tlog_source *source = NULL;
    bool got_pkt = false;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    size_t loc_num;
    char *loc_str = NULL;

    assert(cmd_help != NULL);

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

    /* Get the "follow" flag */
    follow = json_object_object_get_ex(conf, "follow", &obj) &&
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

    /* Setup signal handlers to terminate gracefully */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN)
        {
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

    /* Switch the terminal to raw mode, but keep signal generation */
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | IEXTEN | ECHO);
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
     * Reproduce the logged output
     */
    while (exit_signum == 0) {
        /* Read a packet */
        tlog_pkt_cleanup(&pkt);
        loc_num = tlog_source_loc_get(source);
        grc = tlog_source_read(source, &pkt);
        if (grc == TLOG_GRC_FROM(errno, EINTR)) {
            break;
        } else if (grc != TLOG_RC_OK) {
            tlog_errs_pushc(perrs, grc);
            loc_str = tlog_source_loc_fmt(source, loc_num);
            tlog_errs_pushf(perrs, "Failed reading the source at %s", loc_str);
            goto cleanup;
        }
        /* If hit the end of stream */
        if (tlog_pkt_is_void(&pkt)) {
            if (follow) {
                if (sleep(POLL_PERIOD) != 0) {
                    break;
                }
                continue;
            } else {
                break;
            }
        }
        /* If it's not the output */
        if (pkt.type != TLOG_PKT_TYPE_IO || !pkt.data.io.output) {
            continue;
        }

        /* Get current time */
        if (clock_gettime(CLOCK_MONOTONIC, &local_this_ts) != 0) {
            grc = TLOG_GRC_ERRNO;
            tlog_errs_pushc(perrs, grc);
            tlog_errs_pushs(perrs, "Failed retrieving current time");
            goto cleanup;
        }

        /* If this is the first packet */
        if (!got_pkt) {
            got_pkt = true;
            /* Start the time */
            local_last_ts = local_this_ts;
        } else {
            tlog_timespec_sub(&pkt.timestamp, &pkt_last_ts, &pkt_delay_ts);
            tlog_timespec_add(&local_last_ts, &pkt_delay_ts, &local_next_ts);
            /* If we don't need a delay for the next packet (it's overdue) */
            if (tlog_timespec_cmp(&local_next_ts, &local_this_ts) <= 0) {
                /* Stretch the time */
                local_last_ts = local_this_ts;
            } else {
                rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                     &local_next_ts, NULL);
                if (rc == EINTR) {
                    break;
                } else if (rc != 0) {
                    grc = TLOG_GRC_ERRNO;
                    tlog_errs_pushc(perrs, grc);
                    tlog_errs_pushs(perrs, "Failed sleeping");
                    goto cleanup;
                }
                local_last_ts = local_next_ts;
            }
        }

        /* Output the packet */
        if (write(STDOUT_FILENO, pkt.data.io.buf, pkt.data.io.len) !=
                (ssize_t)pkt.data.io.len) {
            if (errno == EINTR) {
                break;
            } else if (rc != 0) {
                grc = TLOG_GRC_ERRNO;
                tlog_errs_pushc(perrs, grc);
                tlog_errs_pushs(perrs, "Failed writing output");
                goto cleanup;
            }
        }

        pkt_last_ts = pkt.timestamp;
    }

    grc = TLOG_RC_OK;

cleanup:

    free(loc_str);
    tlog_pkt_cleanup(&pkt);
    tlog_source_destroy(source);
    curl_global_cleanup();

    /* Restore signal handlers */
    for (i = 0; i < TLOG_ARRAY_SIZE(exit_sig); i++) {
        sigaction(exit_sig[i], NULL, &sa);
        if (sa.sa_handler != SIG_IGN) {
            signal(exit_sig[i], SIG_DFL);
        }
    }

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
