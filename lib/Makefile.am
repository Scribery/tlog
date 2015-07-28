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

AM_CPPFLAGS = $(JSON_CFLAGS)

lib_LTLIBRARIES = libtlog.la

libtlog_la_SOURCES = \
    fd_reader.c         \
    fd_writer.c         \
    io.c                \
    misc.c              \
    reader.c            \
    reader_type.c       \
    sink.c              \
    stream.c            \
    syslog_writer.c     \
    utf8.c              \
    writer.c

libtlog_la_LIBADD = $(JSON_LIBS)

TESTS = \
    tlog_fd_reader_test         \
    tlog_sink_test              \
    tlog_stream_btoa_test       \
    tlog_stream_enc_bin_test    \
    tlog_stream_enc_txt_test    \
    tlog_stream_test

check_PROGRAMS = \
    tlog_fd_reader_test         \
    tlog_sink_test              \
    tlog_stream_btoa_test       \
    tlog_stream_enc_bin_test    \
    tlog_stream_enc_txt_test    \
    tlog_stream_test

tlog_stream_btoa_test_SOURCES = \
    stream_btoa_test.c

tlog_stream_btoa_test_LDADD = libtlog.la

tlog_stream_enc_bin_test_SOURCES = \
    stream_enc_bin_test.c           \
    stream_enc_test.c               \
    stream_enc_test.h               \
    test.c                          \
    test.h

tlog_stream_enc_bin_test_LDADD = libtlog.la

tlog_stream_enc_txt_test_SOURCES = \
    stream_enc_test.c               \
    stream_enc_test.h               \
    stream_enc_txt_test.c           \
    test.c                          \
    test.h

tlog_stream_enc_txt_test_LDADD = libtlog.la

tlog_stream_test_SOURCES = \
    stream_test.c           \
    test.c                  \
    test.h

tlog_stream_test_LDADD = libtlog.la

tlog_sink_test_SOURCES = \
    sink_test.c             \
    test.c                  \
    test.h

tlog_sink_test_LDADD = libtlog.la

tlog_fd_reader_test_SOURCES = \
    fd_reader_test.c            \
    test.c                      \
    test.h

tlog_fd_reader_test_LDADD = libtlog.la $(JSON_LIBS)
