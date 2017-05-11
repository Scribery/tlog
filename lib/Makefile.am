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
    -DTLOG_PLAY_CONF_DEFAULT_BUILD_PATH='"$(TLOG_PLAY_CONF_DEFAULT_BUILD_PATH)"'                \
    -DTLOG_PLAY_CONF_DEFAULT_INST_PATH='"$(TLOG_PLAY_CONF_DEFAULT_INST_PATH)"'                  \
    -DTLOG_PLAY_CONF_LOCAL_BUILD_PATH='"$(TLOG_PLAY_CONF_LOCAL_BUILD_PATH)"'                    \
    -DTLOG_PLAY_CONF_LOCAL_INST_PATH='"$(TLOG_PLAY_CONF_LOCAL_INST_PATH)"'                      \
    -DTLOG_REC_CONF_DEFAULT_BUILD_PATH='"$(TLOG_REC_CONF_DEFAULT_BUILD_PATH)"'                  \
    -DTLOG_REC_CONF_DEFAULT_INST_PATH='"$(TLOG_REC_CONF_DEFAULT_INST_PATH)"'                    \
    -DTLOG_REC_CONF_LOCAL_BUILD_PATH='"$(TLOG_REC_CONF_LOCAL_BUILD_PATH)"'                      \
    -DTLOG_REC_CONF_LOCAL_INST_PATH='"$(TLOG_REC_CONF_LOCAL_INST_PATH)"'                        \
    -DTLOG_REC_SESSION_CONF_DEFAULT_BUILD_PATH='"$(TLOG_REC_SESSION_CONF_DEFAULT_BUILD_PATH)"'  \
    -DTLOG_REC_SESSION_CONF_DEFAULT_INST_PATH='"$(TLOG_REC_SESSION_CONF_DEFAULT_INST_PATH)"'    \
    -DTLOG_REC_SESSION_CONF_LOCAL_BUILD_PATH='"$(TLOG_REC_SESSION_CONF_LOCAL_BUILD_PATH)"'      \
    -DTLOG_REC_SESSION_CONF_LOCAL_INST_PATH='"$(TLOG_REC_SESSION_CONF_LOCAL_INST_PATH)"'        \
    $(JSON_CFLAGS)                                                                              \
    $(LIBCURL_CPPFLAGS)

lib_LTLIBRARIES = libtlog.la

noinst_LTLIBRARIES = libtlog_test.la

dist_noinst_DATA = \
    conf_validate.c.m4          \
    play_conf_cmd.c.m4          \
    rec_conf_cmd.c.m4           \
    rec_session_conf_cmd.c.m4

PROG_CONF_DEPS = \
    $(top_srcdir)/m4/tlog/misc.m4       \
    $(top_srcdir)/m4/tlog/conf_cmd.m4

REC_CONF_DEPS = \
    $(PROG_CONF_DEPS)                           \
    $(top_srcdir)/m4/tlog/rec_conf_schema.m4

REC_SESSION_CONF_DEPS = \
    $(PROG_CONF_DEPS)                                   \
    $(top_srcdir)/m4/tlog/rec_session_conf_schema.m4

PLAY_CONF_DEPS = \
    $(PROG_CONF_DEPS)                           \
    $(top_srcdir)/m4/tlog/play_conf_schema.m4

play_conf_cmd.c: play_conf_cmd.c.m4 $(PLAY_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

rec_conf_cmd.c: rec_conf_cmd.c.m4 $(REC_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

rec_session_conf_cmd.c: rec_session_conf_cmd.c.m4 $(REC_SESSION_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

play_conf_validate.c: conf_validate.c.m4 $(PLAY_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

rec_conf_validate.c: conf_validate.c.m4 $(REC_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

rec_session_conf_validate.c: conf_validate.c.m4 $(REC_SESSION_CONF_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   --prefix-builtins $< > $@ || \
	{ rm $@ && false; }

BUILT_SOURCES = \
    play_conf_cmd.c             \
    play_conf_validate.c        \
    rec_conf_cmd.c              \
    rec_conf_validate.c         \
    rec_session_conf_cmd.c      \
    rec_session_conf_validate.c

CLEANFILES = $(BUILT_SOURCES)

libtlog_la_SOURCES = \
    delay.c                     \
    errs.c                      \
    es_json_reader.c            \
    fd_json_reader.c            \
    fd_json_writer.c            \
    grc.c                       \
    json_chunk.c                \
    json_dispatcher.c           \
    json_misc.c                 \
    json_msg.c                  \
    json_reader.c               \
    json_sink.c                 \
    json_source.c               \
    json_stream.c               \
    json_writer.c               \
    mem_json_reader.c           \
    mem_json_writer.c           \
    misc.c                      \
    pkt.c                       \
    play_conf.c                 \
    play_conf_cmd.c             \
    play_conf_validate.c        \
    rc.c                        \
    rec_conf.c                  \
    rec_conf_cmd.c              \
    rec_conf_validate.c         \
    rec_session_conf.c          \
    rec_session_conf_cmd.c      \
    rec_session_conf_validate.c \
    sink.c                      \
    source.c                    \
    syslog_json_writer.c        \
    syslog_misc.c               \
    timespec.c                  \
    tty_sink.c                  \
    tty_source.c                \
    utf8.c

libtlog_la_LIBADD = $(JSON_LIBS) $(LIBCURL) -lrt

libtlog_test_la_SOURCES = \
    test_json_sink.c        \
    test_json_source.c      \
    test_json_stream_enc.c  \
    test_misc.c

libtlog_test_la_LIBADD = libtlog.la
