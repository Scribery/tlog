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

tlogdir = $(includedir)/tlog

tlog_HEADERS = \
    conf_origin.h               \
    delay.h                     \
    errs.h                      \
    es_json_reader.h            \
    fd_json_reader.h            \
    fd_json_writer.h            \
    grc.h                       \
    json_chunk.h                \
    json_dispatcher.h           \
    json_misc.h                 \
    json_msg.h                  \
    json_reader.h               \
    json_reader_type.h          \
    json_sink.h                 \
    json_source.h               \
    json_stream.h               \
    json_writer.h               \
    json_writer_type.h          \
    mem_json_reader.h           \
    mem_json_writer.h           \
    misc.h                      \
    pkt.h                       \
    play.h                      \
    play_conf.h                 \
    play_conf_cmd.h             \
    play_conf_validate.h        \
    rc.h                        \
    rec.h                       \
    rec_conf.h                  \
    rec_conf_cmd.h              \
    rec_conf_validate.h         \
    rec_item.h                  \
    rec_session_conf.h          \
    rec_session_conf_cmd.h      \
    rec_session_conf_validate.h \
    rl_json_writer.h            \
    session.h                   \
    sink.h                      \
    sink_type.h                 \
    source.h                    \
    source_type.h               \
    syslog_json_writer.h        \
    syslog_misc.h               \
    tap.h                       \
    timespec.h                  \
    timestr.h                   \
    trx.h                       \
    trx_act.h                   \
    trx_basic.h                 \
    trx_frame.h                 \
    trx_iface.h                 \
    trx_level.h                 \
    trx_state.h                 \
    tty_sink.h                  \
    tty_source.h                \
    utf8.h

if TLOG_JOURNAL_ENABLED
tlog_HEADERS += \
    journal_json_reader.h   \
    journal_json_writer.h   \
    journal_misc.h
endif
