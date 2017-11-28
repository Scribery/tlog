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
    -DTLOG_SESSION_LOCK_DIR='"@localstatedir@/run/tlog"'                                        \
    $(JSON_CFLAGS)                                                                              \
    $(SYSTEMD_JOURNAL_CFLAGS)                                                                   \
    $(LIBCURL_CPPFLAGS)

lib_LTLIBRARIES = libtlog.la

dist_noinst_DATA = \
    conf_validate.c.m4          \
    play_conf_cmd.c.m4          \
    rec_conf_cmd.c.m4           \
    rec_session_conf_cmd.c.m4

TLOG_CONF_DEPS += $(top_srcdir)/m4/tlog/conf_cmd.m4

play_conf_cmd.c: play_conf_cmd.c.m4 $(TLOG_PLAY_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   $< > $@ || \
	{ rm $@ && false; }

rec_conf_cmd.c: rec_conf_cmd.c.m4 $(TLOG_REC_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   $< > $@ || \
	{ rm $@ && false; }

rec_session_conf_cmd.c: rec_session_conf_cmd.c.m4 $(TLOG_REC_SESSION_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   $< > $@ || \
	{ rm $@ && false; }

play_conf_validate.c: conf_validate.c.m4 $(TLOG_PLAY_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   $< > $@ || \
	{ rm $@ && false; }

rec_conf_validate.c: conf_validate.c.m4 $(TLOG_REC_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   $< > $@ || \
	{ rm $@ && false; }

rec_session_conf_validate.c: conf_validate.c.m4 $(TLOG_REC_SESSION_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   $< > $@ || \
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
    play.c                      \
    play_conf.c                 \
    play_conf_cmd.c             \
    play_conf_validate.c        \
    rc.c                        \
    rec.c                       \
    rec_conf.c                  \
    rec_conf_cmd.c              \
    rec_conf_validate.c         \
    rec_item.c                  \
    rec_session_conf.c          \
    rec_session_conf_cmd.c      \
    rec_session_conf_validate.c \
    rl_json_writer.c            \
    session.c                   \
    sink.c                      \
    source.c                    \
    syslog_json_writer.c        \
    syslog_misc.c               \
    tap.c                       \
    timespec.c                  \
    timestr.c                   \
    tty_sink.c                  \
    tty_source.c                \
    utf8.c

if TLOG_JOURNAL_ENABLED
libtlog_la_SOURCES += \
    journal_json_reader.c   \
    journal_json_writer.c   \
    journal_misc.c
endif

libtlog_la_LIBADD = \
    $(JSON_LIBS) \
    $(SYSTEMD_JOURNAL_LIBS) \
    $(LIBCURL) \
    -lutil \
    -lrt \
    -lm
