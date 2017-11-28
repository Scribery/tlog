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

$(TLOG_PLAY_CONF_LOCAL_NAME): $(srcdir)/tlog-prog.conf.m4 $(TLOG_PLAY_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   -D M4_CONF_TYPE=system-wide \
	   -D M4_COMMENT_OUT=true \
	   $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_CONF_LOCAL_NAME): $(srcdir)/tlog-prog.conf.m4 $(TLOG_REC_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   -D M4_CONF_TYPE=system-wide \
	   -D M4_COMMENT_OUT=true \
	   $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_SESSION_CONF_LOCAL_NAME): $(srcdir)/tlog-prog.conf.m4 $(TLOG_REC_SESSION_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   -D M4_CONF_TYPE=system-wide \
	   -D M4_COMMENT_OUT=true \
	   $< > $@ || \
	{ rm $@ && false; }

$(TLOG_PLAY_CONF_DEFAULT_NAME): $(srcdir)/tlog-prog.conf.m4 $(TLOG_PLAY_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   -D M4_CONF_TYPE=default \
	   $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_CONF_DEFAULT_NAME): $(srcdir)/tlog-prog.conf.m4 $(TLOG_REC_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   -D M4_CONF_TYPE=default \
	   $< > $@ || \
	{ rm $@ && false; }

$(TLOG_REC_SESSION_CONF_DEFAULT_NAME): $(srcdir)/tlog-prog.conf.m4 $(TLOG_REC_SESSION_CONF_DEPS)
	m4 $(M4FLAGS) \
	   -D M4_PROG_NAME=rec-session \
	   -D M4_PROG_SYM=rec_session \
	   -D M4_CONF_TYPE=default \
	   $< > $@ || \
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
    $(JSON_CFLAGS)      \
    $(LIBCURL_CPPFLAGS)

bin_PROGRAMS = \
    tlog-rec            \
    tlog-rec-session    \
    tlog-play

tlog_rec_SOURCES = \
    tlog-rec.c
tlog_rec_CFLAGS = \
    $(PTHREAD_CFLAGS)
tlog_rec_LDADD = \
    ../../lib/tlog/libtlog.la   \
    $(JSON_LIBS)                \
    $(PTHREAD_LIBS)             \
    -lrt                        \
    -lutil

tlog_rec_session_SOURCES = \
    tlog-rec-session.c
tlog_rec_session_CFLAGS = \
    $(PTHREAD_CFLAGS)
tlog_rec_session_LDADD = \
    ../../lib/tlog/libtlog.la   \
    $(JSON_LIBS)                \
    $(PTHREAD_LIBS)             \
    -lrt                        \
    -lutil

tlog_play_SOURCES = \
    tlog-play.c
tlog_play_LDADD = \
    ../../lib/tlog/libtlog.la   \
    $(JSON_LIBS)                \
    $(LIBCURL)                  \
    -lrt                        \
    -lm
