#
# Copyright (C) 2015 Red Hat
#
# This file is part of tlog.
#
# Tlog is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Tlog is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with tlog; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

include $(top_srcdir)/Common.am

AM_CPPFLAGS = \
    $(JSON_CFLAGS)      \
    $(LIBCURL_CPPFLAGS)

TESTS = \
    tltest-fd-json-reader       \
    tltest-grc                  \
    tltest-json-esc             \
    tltest-json-overlay         \
    tltest-json-passthrough     \
    tltest-json-sink            \
    tltest-json-source          \
    tltest-json-stream          \
    tltest-json-stream-btoa     \
    tltest-json-stream-enc-bin  \
    tltest-json-stream-enc-txt  \
    tltest-timespec             \
    tltest-timestr

check_PROGRAMS = \
    tltest-fd-json-reader       \
    tltest-grc                  \
    tltest-json-esc             \
    tltest-json-overlay         \
    tltest-json-passthrough     \
    tltest-json-sink            \
    tltest-json-source          \
    tltest-json-stream          \
    tltest-json-stream-btoa     \
    tltest-json-stream-enc-bin  \
    tltest-json-stream-enc-txt  \
    tltest-timespec             \
    tltest-timestr

tltest_json_stream_btoa_SOURCES = tltest-json-stream-btoa.c
tltest_json_stream_btoa_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la

tltest_json_stream_enc_bin_SOURCES = tltest-json-stream-enc-bin.c
tltest_json_stream_enc_bin_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la

tltest_json_stream_enc_txt_SOURCES = tltest-json-stream-enc-txt.c
tltest_json_stream_enc_txt_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la

tltest_json_stream_SOURCES = tltest-json-stream.c
tltest_json_stream_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la

tltest_json_sink_SOURCES = tltest-json-sink.c
tltest_json_sink_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la

tltest_fd_json_reader_SOURCES = tltest-fd-json-reader.c
tltest_fd_json_reader_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la       \
    $(JSON_LIBS)

tltest_grc_SOURCES = tltest-grc.c
tltest_grc_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la       \
    $(JSON_LIBS)                    \
    $(LIBCURL)

tltest_json_source_SOURCES = tltest-json-source.c
tltest_json_source_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la       \
    $(JSON_LIBS)

tltest_json_passthrough_SOURCES = tltest-json-passthrough.c
tltest_json_passthrough_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la       \
    $(JSON_LIBS)

tltest_json_overlay_SOURCES = tltest-json-overlay.c
tltest_json_overlay_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la       \
    $(JSON_LIBS)

tltest_json_esc_SOURCES = tltest-json-esc.c
tltest_json_esc_LDADD = \
    ../../lib/tltest/libtltest.la   \
    ../../lib/tlog/libtlog.la

tltest_timespec_SOURCES = tltest-timespec.c
tltest_timespec_LDADD = \
    ../../lib/tlog/libtlog.la   \
    -lm

tltest_timestr_SOURCES = tltest-timestr.c
tltest_timestr_LDADD = \
    ../../lib/tlog/libtlog.la
