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

AM_CPPFLAGS = $(JSON_CFLAGS) $(LIBCURL_CPPFLAGS)

lib_LTLIBRARIES = libtlog.la

noinst_LTLIBRARIES = libtlog_test.la

libtlog_la_SOURCES = \
    chunk.c             \
    es_reader.c         \
    fd_reader.c         \
    fd_writer.c         \
    grc.c               \
    mem_reader.c        \
    mem_writer.c        \
    misc.c              \
    msg.c               \
    pkt.c               \
    rc.c                \
    reader.c            \
    reader_type.c       \
    sink.c              \
    source.c            \
    stream.c            \
    syslog_writer.c     \
    utf8.c              \
    writer.c

libtlog_la_LIBADD = $(JSON_LIBS) $(LIBCURL)

libtlog_test_la_SOURCES = \
    test_misc.c             \
    test_sink.c             \
    test_source.c           \
    test_stream_enc.c

libtlog_test_la_LIBADD = libtlog.la
