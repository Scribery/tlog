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

dist_noinst_DATA = \
    tlog-prog.conf.m4

pkgconflocal_DATA = \
    $(TLOG_PLAY_CONF_LOCAL_NAME)        \
    $(TLOG_REC_CONF_LOCAL_NAME)         \
    $(TLOG_REC_SESSION_CONF_LOCAL_NAME)

pkgconfdefault_DATA = \
    $(TLOG_PLAY_CONF_DEFAULT_NAME)          \
    $(TLOG_REC_CONF_DEFAULT_NAME)           \
    $(TLOG_REC_SESSION_CONF_DEFAULT_NAME)

CLEANFILES = \
    $(TLOG_PLAY_CONF_DEFAULT_NAME)          \
    $(TLOG_PLAY_CONF_LOCAL_NAME)            \
    $(TLOG_REC_CONF_DEFAULT_NAME)           \
    $(TLOG_REC_CONF_LOCAL_NAME)             \
    $(TLOG_REC_SESSION_CONF_DEFAULT_NAME)   \
    $(TLOG_REC_SESSION_CONF_LOCAL_NAME)

CONF_DEPS = \
    $(srcdir)/tlog-prog.conf.m4             \
    $(top_srcdir)/m4/tlog/conf_origin.m4    \
    $(top_srcdir)/m4/tlog/conf_schema.m4    \
    $(top_srcdir)/m4/tlog/misc.m4

REC_CONF_DEPS = \
    $(CONF_DEPS)                                    \
    $(top_srcdir)/m4/tlog/rec_common_conf_schema.m4 \
    $(top_srcdir)/m4/tlog/rec_conf_schema.m4

REC_SESSION_CONF_DEPS = \
    $(CONF_DEPS)                                        \
    $(top_srcdir)/m4/tlog/rec_common_conf_schema.m4     \
    $(top_srcdir)/m4/tlog/rec_session_conf_schema.m4

PLAY_CONF_DEPS = \
    $(CONF_DEPS)                                \
    $(top_srcdir)/m4/tlog/play_conf_schema.m4

$(TLOG_PLAY_CONF_LOCAL_NAME): $(PLAY_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   -D M4_CONF_TYPE=system-wide \
	   -D M4_COMMENT_OUT=true \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_CONF_LOCAL_NAME): $(REC_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   -D M4_CONF_TYPE=system-wide \
	   -D M4_COMMENT_OUT=true \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_SESSION_CONF_LOCAL_NAME): $(REC_SESSION_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   -D M4_CONF_TYPE=system-wide \
	   -D M4_COMMENT_OUT=true \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

$(TLOG_PLAY_CONF_DEFAULT_NAME): $(PLAY_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   -D M4_CONF_TYPE=default \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_CONF_DEFAULT_NAME): $(REC_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   -D M4_CONF_TYPE=default \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_SESSION_CONF_DEFAULT_NAME): $(REC_SESSION_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   -D M4_CONF_TYPE=default \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

install-data-hook:
	: Remove "DO NOT EDIT" warning
	$(SED) -i -e '/vim:nomodifiable/,/\*\{73\}/ d' \
	       $(DESTDIR)$(TLOG_PLAY_CONF_DEFAULT_INST_PATH)        \
	       $(DESTDIR)$(TLOG_PLAY_CONF_LOCAL_INST_PATH)          \
	       $(DESTDIR)$(TLOG_REC_CONF_DEFAULT_INST_PATH)         \
	       $(DESTDIR)$(TLOG_REC_CONF_LOCAL_INST_PATH)           \
	       $(DESTDIR)$(TLOG_REC_SESSION_CONF_DEFAULT_INST_PATH) \
	       $(DESTDIR)$(TLOG_REC_SESSION_CONF_LOCAL_INST_PATH)

AM_CPPFLAGS = \
    $(JSON_CFLAGS) \
    $(LIBCURL_CPPFLAGS) \
    -DTLOG_SESSION_LOCK_DIR='"@localstatedir@/run/tlog"'

bin_PROGRAMS = \
    tlog-rec            \
    tlog-rec-session    \
    tlog-play

tlog_rec_SOURCES = \
    tlog-rec.c
tlog_rec_CFLAGS = \
    $(PTHREAD_CFLAGS)
tlog_rec_LDADD = \
    ../lib/libtlog.la   \
    $(JSON_LIBS)        \
    $(PTHREAD_LIBS)     \
    -lrt                \
    -lutil

tlog_rec_session_SOURCES = \
    tlog-rec-session.c
tlog_rec_session_CFLAGS = \
    $(PTHREAD_CFLAGS)
tlog_rec_session_LDADD = \
    ../lib/libtlog.la       \
    $(JSON_LIBS)            \
    $(PTHREAD_LIBS)         \
    -lrt                    \
    -lutil

tlog_play_SOURCES = \
    tlog-play.c
tlog_play_LDADD = \
    ../lib/libtlog.la   \
    $(JSON_LIBS)        \
    $(LIBCURL)          \
    -lrt

TESTS = \
    tlog-test-fd-json-reader        \
    tlog-test-grc                   \
    tlog-test-json-esc              \
    tlog-test-json-overlay          \
    tlog-test-json-passthrough      \
    tlog-test-json-sink             \
    tlog-test-json-source           \
    tlog-test-json-stream           \
    tlog-test-json-stream-btoa      \
    tlog-test-json-stream-enc-bin   \
    tlog-test-json-stream-enc-txt

check_PROGRAMS = \
    tlog-test-fd-json-reader        \
    tlog-test-grc                   \
    tlog-test-json-esc              \
    tlog-test-json-overlay          \
    tlog-test-json-passthrough      \
    tlog-test-json-sink             \
    tlog-test-json-source           \
    tlog-test-json-stream           \
    tlog-test-json-stream-btoa      \
    tlog-test-json-stream-enc-bin   \
    tlog-test-json-stream-enc-txt

tlog_test_json_stream_btoa_SOURCES = tlog-test-json-stream-btoa.c
tlog_test_json_stream_btoa_LDADD = \
    ../lib/libtlog_test.la      \
    ../lib/libtlog.la

tlog_test_json_stream_enc_bin_SOURCES = tlog-test-json-stream-enc-bin.c
tlog_test_json_stream_enc_bin_LDADD = \
    ../lib/libtlog_test.la          \
    ../lib/libtlog.la

tlog_test_json_stream_enc_txt_SOURCES = tlog-test-json-stream-enc-txt.c
tlog_test_json_stream_enc_txt_LDADD = \
    ../lib/libtlog_test.la          \
    ../lib/libtlog.la

tlog_test_json_stream_SOURCES = tlog-test-json-stream.c
tlog_test_json_stream_LDADD = \
    ../lib/libtlog_test.la  \
    ../lib/libtlog.la

tlog_test_json_sink_SOURCES = tlog-test-json-sink.c
tlog_test_json_sink_LDADD = \
    ../lib/libtlog_test.la  \
    ../lib/libtlog.la

tlog_test_fd_json_reader_SOURCES = tlog-test-fd-json-reader.c
tlog_test_fd_json_reader_LDADD = \
    ../lib/libtlog_test.la  \
    ../lib/libtlog.la       \
    $(JSON_LIBS)

tlog_test_grc_SOURCES = tlog-test-grc.c
tlog_test_grc_LDADD = \
    ../lib/libtlog_test.la  \
    ../lib/libtlog.la       \
    $(JSON_LIBS)            \
    $(LIBCURL)

tlog_test_json_source_SOURCES = tlog-test-json-source.c
tlog_test_json_source_LDADD = \
    ../lib/libtlog_test.la  \
    ../lib/libtlog.la       \
    $(JSON_LIBS)

tlog_test_json_passthrough_SOURCES = tlog-test-json-passthrough.c
tlog_test_json_passthrough_LDADD = \
    ../lib/libtlog_test.la  \
    ../lib/libtlog.la       \
    $(JSON_LIBS)

tlog_test_json_overlay_SOURCES = tlog-test-json-overlay.c
tlog_test_json_overlay_LDADD = \
    ../lib/libtlog_test.la      \
    ../lib/libtlog.la           \
    $(JSON_LIBS)

tlog_test_json_esc_SOURCES = tlog-test-json-esc.c
tlog_test_json_esc_LDADD = \
    ../lib/libtlog_test.la      \
    ../lib/libtlog.la
