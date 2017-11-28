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
    $(JSON_CFLAGS)                                                                              \
    $(SYSTEMD_JOURNAL_CFLAGS)                                                                   \
    $(LIBCURL_CPPFLAGS)

noinst_LTLIBRARIES = libtltest.la

libtltest_la_SOURCES = \
    json_sink.c         \
    json_source.c       \
    json_stream_enc.c   \
    misc.c

libtltest_la_LIBADD = ../tlog/libtlog.la
