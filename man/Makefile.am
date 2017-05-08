#
# Copyright (C) 2016 Red Hat
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
    tlog-play.8.m4      \
    tlog-play.conf.5.m4 \
    tlog-rec.8.m4       \
    tlog-rec.conf.5.m4

MAN_DEPS = \
    $(top_srcdir)/m4/tlog/misc.m4   \
    $(top_srcdir)/m4/tlog/man.m4

PLAY_MAN_DEPS = \
	$(MAN_DEPS)                                    \
    $(top_srcdir)/m4/tlog/play_conf_schema.m4

REC_MAN_DEPS = \
	$(MAN_DEPS)                                    \
    $(top_srcdir)/m4/tlog/rec_conf_schema.m4

tlog-play.8: tlog-play.8.m4 $(PLAY_MAN_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_CONF_PATH="$(TLOG_PLAY_CONF_LOCAL_INST_PATH)" \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   --prefix-builtins \
	   $< > $@ || \
	{ rm $@ && false; }

tlog-rec.8: tlog-rec.8.m4 $(REC_MAN_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_CONF_PATH="$(TLOG_REC_CONF_LOCAL_INST_PATH)" \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   --prefix-builtins \
	   $< > $@ || \
	{ rm $@ && false; }

tlog-rec.conf.5: tlog-rec.conf.5.m4 $(REC_MAN_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=rec \
	   -D M4_PROG_SYM=rec \
	   --prefix-builtins \
	   $< > $@ || \
	{ rm $@ && false; }

tlog-play.conf.5: tlog-play.conf.5.m4 $(PLAY_MAN_DEPS)
	m4 -I $(top_srcdir)/m4/tlog \
	   -D M4_PROG_NAME=play \
	   -D M4_PROG_SYM=play \
	   --prefix-builtins \
	   $< > $@ || \
	{ rm $@ && false; }

dist_man_MANS = \
    tlog-play.8         \
    tlog-play.conf.5    \
    tlog-rec.8          \
    tlog-rec.conf.5

CLEANFILES = \
    tlog-play.8         \
    tlog-play.conf.5    \
    tlog-rec.8          \
    tlog-rec.conf.5

install-data-hook:
	: Remove "DO NOT EDIT" warning
	$(SED) -i -e '/vim:nomodifiable/,/\*\{73\}/ d' \
	       $(DESTDIR)$(mandir)/man5/tlog-play.conf.5 \
	       $(DESTDIR)$(mandir)/man5/tlog-rec.conf.5 \
	       $(DESTDIR)$(mandir)/man8/tlog-play.8 \
	       $(DESTDIR)$(mandir)/man8/tlog-rec.8

