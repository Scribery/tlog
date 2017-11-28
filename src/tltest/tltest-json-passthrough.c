/*
 * Tlog pass-through test.
 *
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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <tlog/rc.h>
#include <tlog/grc.h>
#include <tlog/mem_json_writer.h>
#include <tlog/mem_json_reader.h>
#include <tlog/json_sink.h>
#include <tlog/json_source.h>
#include <tlog/delay.h>
#include <tlog/misc.h>
#include <tltest/json_sink.h>
#include <tltest/json_source.h>

struct tltest_json_passthrough {
    struct tltest_json_sink_input    input;
    struct tltest_json_source_output output;
};

static bool
tltest_json_passthrough(const char *file,
                        int line,
                        const char *name,
                        struct tltest_json_passthrough test)
{
    bool passed = true;
    char *sink_name = NULL;
    char *source_name = NULL;
    char *log_buf = NULL;
    size_t log_len = 0;

    if (asprintf(&sink_name, "%s: sink", name) < 0) {
        fprintf(stderr, "Failed formatting sink test name: %s\n",
                strerror(errno));
        passed = false;
        goto exit;
    }
    if (asprintf(&source_name, "%s: source", name) < 0) {
        fprintf(stderr, "Failed formatting source test name: %s\n",
                strerror(errno));
        passed = false;
        goto exit;
    }

    passed = tltest_json_sink_run(sink_name,
                                  &test.input, &log_buf, &log_len) &&
             passed;

    passed = tltest_json_source_run(source_name,
                                    log_buf, log_len, &test.output) &&
             passed;

exit:
    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, name);
    if (!passed) {
        fprintf(stderr, "%s log:\n%.*s", name, (int)log_len, log_buf);
    }

    free(sink_name);
    free(source_name);
    free(log_buf);

    return passed;
}

static bool
tltest_json_passthrough_buf(const char *name,
                            size_t sink_chunk_size,
                            size_t source_io_size,
                            const char *data_buf,
                            size_t data_len)
{
    bool mismatch;
    bool passed = false;
    struct tlog_json_writer *writer = NULL;
    struct tlog_sink *sink = NULL;
    struct tlog_json_reader *reader = NULL;
    struct tlog_source *source = NULL;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    char *log_buf = NULL;
    size_t log_len = 0;
    size_t pkt_pos;
    size_t data_pos;
    uint8_t data_byte;
    uint8_t pkt_byte;

#define GUARD(_op_name, _op_expr) \
    do {                                                        \
        tlog_grc grc;                                           \
        grc = (_op_expr);                                       \
        if (grc != TLOG_RC_OK) {                                \
            fprintf(stderr, "%s: Failed to %s: %s\n",           \
                    name, _op_name, tlog_grc_strerror(grc));    \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

    GUARD("create a memory writer",
          tlog_mem_json_writer_create(&writer, &log_buf, &log_len));
    {
        struct tlog_json_sink_params params = {
            .writer = writer,
            .writer_owned = false,
            .hostname = "localhost",
            .recording = "rec-1",
            .username = "user",
            .terminal = "xterm",
            .session_id = 1,
            .chunk_size = sink_chunk_size,
        };
        GUARD("create a sink", tlog_json_sink_create(&sink, &params));
    }

    pkt = TLOG_PKT_IO(0, 0, true, data_buf, data_len);

    GUARD("write the buffer to the sink",
          tlog_sink_write(sink, &pkt, NULL, NULL));
    GUARD("cut the sink", tlog_sink_cut(sink));
    GUARD("flush the sink", tlog_sink_flush(sink));

    GUARD("create a memory reader",
          tlog_mem_json_reader_create(&reader, log_buf, log_len));
    {
        struct tlog_json_source_params params = {
            .reader = reader,
            .reader_owned = false,
            .io_size = source_io_size,
        };
        GUARD("create a source",
              tlog_json_source_create(&source, &params));
    }

    mismatch = false;
    data_pos = 0;
    while (true) {
        tlog_pkt_cleanup(&pkt);
        GUARD("read a packet from the source",
              tlog_source_read(source, &pkt));
        if (pkt.type == TLOG_PKT_TYPE_VOID) {
            break;
        } else if (pkt.type != TLOG_PKT_TYPE_IO) {
            fprintf(stderr,
                    "%s: Invalid packet type encountered at %zu: %s\n",
                    name, data_pos, tlog_pkt_type_to_str(pkt.type));
            goto cleanup;
        }

        for (pkt_pos = 0; pkt_pos < pkt.data.io.len; pkt_pos++, data_pos++) {
            pkt_byte = pkt.data.io.buf[pkt_pos];
            if (data_pos < data_len) {
                data_byte = data_buf[data_pos];
                if (pkt_byte != data_byte) {
                    fprintf(stderr, "%s: mismatch at %zu: 0x%hhx != 0x%hhx\n",
                            name, data_pos, pkt_byte, data_byte);
                    mismatch = true;
                }
            } else {
                fprintf(stderr, "%s: extra byte at %zu: 0x%hhx\n",
                        name, data_pos, pkt_byte);
                mismatch = true;
            }

        }
    }
    for (; data_pos < data_len; data_pos++) {
        data_byte = data_buf[data_pos];
        fprintf(stderr, "%s: missing byte at %zu: 0x%hhx\n",
                name, data_pos, data_byte);
        mismatch = true;
    }

    passed = !mismatch;

cleanup:
    free(log_buf);
    tlog_source_destroy(source);
    tlog_json_reader_destroy(reader);
    tlog_sink_destroy(sink);
    tlog_json_writer_destroy(writer);
    return passed;
}

static bool
tltest_json_passthrough_random(const char *name,
                               size_t sink_chunk_size,
                               size_t source_io_size,
                               size_t data_len)
{
    bool passed;
    char *data_buf;
    size_t i;

    data_buf = malloc(data_len);
    for (i = 0; i < data_len; i++) {
        data_buf[i] = (uint64_t)random() * 255 / RAND_MAX;
    }

    passed = tltest_json_passthrough_buf(name,
                                         sink_chunk_size, source_io_size,
                                         data_buf, data_len);
    free(data_buf);

    return passed;
}

int
main(void)
{
    bool passed = true;
    char buf[256];
    size_t i;

#define PKT_VOID \
    TLOG_PKT_VOID
#define PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height) \
    TLOG_PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height)
#define PKT_IO(_tv_sec, _tv_nsec, _output, _buf, _len) \
    TLOG_PKT_IO(_tv_sec, _tv_nsec, _output, _buf, _len)
#define PKT_IO_STR(_tv_sec, _tv_nsec, _output, _buf) \
    TLOG_PKT_IO_STR(_tv_sec, _tv_nsec, _output, _buf)

#define OP_WRITE(_pkt)  TLTEST_JSON_SINK_OP_WRITE(_pkt)
#define OP_FLUSH        TLTEST_JSON_SINK_OP_FLUSH
#define OP_CUT          TLTEST_JSON_SINK_OP_CUT

#define OP_LOC_GET(_exp_loc) \
    TLTEST_JSON_SOURCE_OP_LOC_GET(_exp_loc)
#define OP_READ(_exp_grc, _exp_pkt) \
    TLTEST_JSON_SOURCE_OP_READ(_exp_grc, _exp_pkt)
#define OP_READ_OK(_exp_pkt) \
    OP_READ(TLOG_RC_OK, _exp_pkt)

#define INPUT(_struct_init_args...) \
    .input = {                      \
        .hostname = "localhost",    \
        .recording = "rec-1",       \
        .username = "user",         \
        .terminal = "xterm",        \
        .session_id = 1,            \
        _struct_init_args           \
    }

#define OUTPUT(_struct_init_args...) \
    .output = {                         \
        .hostname = NULL,               \
        .username = NULL,               \
        .terminal = NULL,               \
        .session_id = 0,                \
        _struct_init_args               \
    }

#define TEST(_name_token, _struct_init_args...) \
    passed = tltest_json_passthrough(                               \
                __FILE__, __LINE__, #_name_token,                   \
                (struct tltest_json_passthrough){_struct_init_args} \
             ) && passed

    TEST(null,
         INPUT(
            .chunk_size = 32,
            .op_list = {},
         ),
         OUTPUT(
             .io_size = 4,
             .op_list = {
                OP_READ_OK(PKT_VOID)
             }
         )
    );

    TEST(null_flush,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(window,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200))
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(window_flush,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(min_window,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 0, 0)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 0, 0)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(max_window,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, USHRT_MAX, USHRT_MAX)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, USHRT_MAX, USHRT_MAX)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(min_delay_between_windows,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 10, 20)),
                OP_WRITE(PKT_WINDOW(0, 1000000, 30, 40)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 10, 20)),
                OP_READ_OK(PKT_WINDOW(0, 1000000, 30, 40)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(max_delay_between_windows,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 10, 20)),
                OP_WRITE(PKT_WINDOW(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                    TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                    30, 40)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 10, 20)),
                OP_READ_OK(PKT_WINDOW(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                      TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                      30, 40)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(one_char_one_byte,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(one_char_two_bytes,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "Я")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "Я")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(one_char_three_bytes,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xe1\x9a\xa0")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "\xe1\x9a\xa0")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(one_char_four_bytes,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xf0\x9d\x84\x9e")),

                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "\xf0\x9d\x84\x9e")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    for (i = 0; i < sizeof(buf); i++) {
        buf[i] = (char)i;
    }
    TEST(all_bytes_forward_unsplit,
         INPUT(
            .chunk_size = 2048,
            .op_list = {
                OP_WRITE(PKT_IO(0, 0, true, buf, sizeof(buf))),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = sizeof(buf),
            .op_list = {
                OP_READ_OK(PKT_IO(0, 0, true, buf, sizeof(buf))),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    for (i = 0; i < sizeof(buf); i++) {
        buf[i] = (char)(sizeof(buf) - i);
    }
    TEST(all_bytes_backward_unsplit,
         INPUT(
            .chunk_size = 2048,
            .op_list = {
                OP_WRITE(PKT_IO(0, 0, true, buf, sizeof(buf))),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = sizeof(buf),
            .op_list = {
                OP_READ_OK(PKT_IO(0, 0, true, buf, sizeof(buf))),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(packet_merging,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_IO_STR(0, 0, true, "B")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "AB")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(min_delay_between_chars,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_IO_STR(0, 1000000, true, "B")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_IO_STR(0, 1000000, true, "B")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(max_delay_between_chars,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_IO_STR(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                    TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                    true, "B")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_IO_STR(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                      TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                      true, "B")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(min_delay_inside_char,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xf0\x9d")),
                OP_WRITE(PKT_IO_STR(0, 1000000, true, "\x84\x9e")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 1000000, true,
                                      "\xf0\x9d\x84\x9e")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(max_delay_inside_char,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xf0\x9d")),
                OP_WRITE(PKT_IO_STR(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                    TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                    true, "\x84\x9e")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                      TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                      true, "\xf0\x9d\x84\x9e")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(windows_and_chars,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_WINDOW(0, 0, 300, 400)),
                OP_WRITE(PKT_IO_STR(0, 0, true, "B")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_WINDOW(0, 0, 300, 400)),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "B")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(chars_and_windows,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_WRITE(PKT_IO_STR(0, 0, true, "B")),
                OP_WRITE(PKT_WINDOW(0, 0, 300, 400)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "B")),
                OP_READ_OK(PKT_WINDOW(0, 0, 300, 400)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(input_and_output_merged,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, false, "A")),
                OP_WRITE(PKT_IO_STR(0, 0, true, "B")),
                OP_WRITE(PKT_IO_STR(0, 0, false, "C")),
                OP_WRITE(PKT_IO_STR(0, 0, true, "D")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, false, "AC")),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "BD")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(output_and_input_merged,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_IO_STR(0, 0, false, "B")),
                OP_WRITE(PKT_IO_STR(0, 0, true, "C")),
                OP_WRITE(PKT_IO_STR(0, 0, false, "D")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, false, "BD")),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "AC")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(input_and_output_separate,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, false, "A")),
                OP_WRITE(PKT_IO_STR(1, 0, true, "B")),
                OP_WRITE(PKT_IO_STR(2, 0, false, "C")),
                OP_WRITE(PKT_IO_STR(3, 0, true, "D")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, false, "A")),
                OP_READ_OK(PKT_IO_STR(1, 0, true, "B")),
                OP_READ_OK(PKT_IO_STR(2, 0, false, "C")),
                OP_READ_OK(PKT_IO_STR(3, 0, true, "D")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(output_and_input_separate,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_IO_STR(1, 0, false, "B")),
                OP_WRITE(PKT_IO_STR(2, 0, true, "C")),
                OP_WRITE(PKT_IO_STR(3, 0, false, "D")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_IO_STR(1, 0, false, "B")),
                OP_READ_OK(PKT_IO_STR(2, 0, true, "C")),
                OP_READ_OK(PKT_IO_STR(3, 0, false, "D")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(window_merging_immediate,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(window_merging_over_char,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_WRITE(PKT_IO_STR(0, 0, true, "A")),
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "A")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(string_flushed,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "AB")),
                OP_FLUSH,
                OP_WRITE(PKT_IO_STR(0, 0, true, "CD")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "AB")),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "CD")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(string_cut,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "AB")),
                OP_CUT,
                OP_WRITE(PKT_IO_STR(0, 0, true, "CD")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "ABCD")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(char_flushed,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xf0\x9d")),
                OP_FLUSH,
                OP_WRITE(PKT_IO_STR(0, 0, true, "\x84\x9e")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "\xf0\x9d\x84\x9e")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(char_cut,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xf0\x9d")),
                OP_CUT,
                OP_WRITE(PKT_IO_STR(0, 0, true, "\x84\x9e")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "\xf0\x9d\x84\x9e")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(char_cut_and_flushed,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true, "\xf0\x9d")),
                OP_CUT,
                OP_FLUSH,
                OP_WRITE(PKT_IO_STR(0, 0, true, "\x84\x9e")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 4,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "\xf0\x9d")),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "\x84\x9e")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(chinese,
         INPUT(
            .chunk_size = 2048,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true,
                                    "自河南经乱关内阻饥兄弟离散各在一处…"
                                    "符离及下邽弟妹"
                                    ""
                                    "时难年饥世业空"
                                    "弟兄羁旅各西东"
                                    "田园寥落干戈后"
                                    "骨肉流离道路中"
                                    "吊影分为千里雁"
                                    "辞根散作九秋蓬"
                                    "共看明月应垂泪"
                                    "一夜乡心五处同")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 1024,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "自河南经乱关内阻饥兄弟离散各在一处…"
                                      "符离及下邽弟妹"
                                      ""
                                      "时难年饥世业空"
                                      "弟兄羁旅各西东"
                                      "田园寥落干戈后"
                                      "骨肉流离道路中"
                                      "吊影分为千里雁"
                                      "辞根散作九秋蓬"
                                      "共看明月应垂泪"
                                      "一夜乡心五处同")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(input_splitting,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true,
                                    "0123456789abcdef0123456789abc"
                                    "0123456789abcdef0123456789abc")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 58,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "0123456789abcdef0123456789abc")),
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "0123456789abcdef0123456789abc")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(output_splitting,
         INPUT(
            .chunk_size = 35,
            .op_list = {
                OP_WRITE(PKT_IO_STR(0, 0, true,
                                    "0123456789abcdef0123456789abcdef")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 16,
            .op_list = {
                OP_READ_OK(PKT_IO_STR(0, 0, true, "0123456789abcdef")),
                OP_READ_OK(PKT_IO_STR(0, 0, true, "0123456789abcdef")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(window_repeat_suppressed,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_WRITE(PKT_IO_STR(0, 0, true,
                                    "0123456789abcdef01234"
                                    "0123456789abcdef01234")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 58,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "0123456789abcdef01234")),
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "0123456789abcdef01234")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    TEST(window_not_suppressed,
         INPUT(
            .chunk_size = 32,
            .op_list = {
                OP_WRITE(PKT_WINDOW(0, 0, 100, 200)),
                OP_WRITE(PKT_IO_STR(0, 0, true,
                                    "0123456789abcdef01234")),
                OP_WRITE(PKT_WINDOW(0, 0, 200, 300)),
                OP_WRITE(PKT_IO_STR(0, 0, true,
                                    "0123456789abcdef01234")),
                OP_FLUSH
            },
         ),
         OUTPUT(
            .io_size = 58,
            .op_list = {
                OP_READ_OK(PKT_WINDOW(0, 0, 100, 200)),
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "0123456789abcdef01234")),
                OP_READ_OK(PKT_WINDOW(0, 0, 200, 300)),
                OP_READ_OK(PKT_IO_STR(0, 0, true,
                                      "0123456789abcdef01234")),
                OP_READ_OK(PKT_VOID)
            }
         )
    );

    passed = tltest_json_passthrough_random("random",
                                            16 * 1024, 16 * 1024,
                                            256 * 1024) &&
             passed;

    return !passed;
}
