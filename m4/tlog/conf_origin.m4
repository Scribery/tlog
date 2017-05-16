m4_dnl
m4_dnl Configuration parameter origin m4 macros.
m4_dnl
m4_dnl Configuration parameter origin is one of the following strings:
m4_dnl "file", "env", "name", "opts", or "args".
m4_dnl
m4_dnl Configuration parameter origin range is either an empty string for all origins,
m4_dnl ORIGIN for specific origin, ORIGIN- for specific origin and all that follows,
m4_dnl -ORIGIN for specific origin and all that precedes, or ORIGIN1-ORIGIN2 for
m4_dnl everything between and including two specific origins.
m4_dnl
m4_dnl Copyright (C) 2017 Red Hat
m4_dnl
m4_dnl This file is part of tlog.
m4_dnl
m4_dnl Tlog is free software; you can redistribute it and/or modify
m4_dnl it under the terms of the GNU General Public License as published by
m4_dnl the Free Software Foundation; either version 2 of the License, or
m4_dnl (at your option) any later version.
m4_dnl
m4_dnl Tlog is distributed in the hope that it will be useful,
m4_dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
m4_dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
m4_dnl GNU General Public License for more details.
m4_dnl
m4_dnl You should have received a copy of the GNU General Public License
m4_dnl along with tlog; if not, write to the Free Software
m4_dnl Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
m4_dnl
m4_include(`misc.m4')m4_dnl
m4_divert(-1)

m4_dnl
m4_dnl Expand to the minimum possible origin.
m4_dnl
m4_define(`m4_conf_origin_min', `file')

m4_dnl
m4_dnl Expand to the maximum possible origin.
m4_dnl
m4_define(`m4_conf_origin_max', `args')

m4_dnl
m4_dnl Expand to the origin value, or 0 for invalid origin.
m4_dnl
m4_dnl      $1 An origin
m4_dnl
m4_define(`m4_conf_origin_val',
          `m4_ifelse(`$1', `file',  1,
                     `$1', `env',   2,
                     `$1', `name',  3,
                     `$1', `opts',  4,
                     `$1', `args',  5,
                     0)')

m4_dnl
m4_dnl Expand to the origin C symbol, or to TLOG_CONF_ORIGIN_UNKNOWN for
m4_dnl invalid origin.
m4_dnl
m4_dnl      $1 An origin
m4_dnl
m4_define(`m4_conf_origin_sym',
          `m4_ifelse(`$1', `file',  TLOG_CONF_ORIGIN_FILE,
                     `$1', `env',   TLOG_CONF_ORIGIN_ENV,
                     `$1', `name',  TLOG_CONF_ORIGIN_NAME,
                     `$1', `opts',  TLOG_CONF_ORIGIN_OPTS,
                     `$1', `args',  TLOG_CONF_ORIGIN_ARGS,
                     TLOG_CONF_ORIGIN_UNKNOWN)')

m4_dnl
m4_dnl Expand to the minimum from an origin range.
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 An origin range
m4_dnl
m4_define(`m4_conf_origin_range_min',
          `m4_repl_cases(m4_regexp(`$1', `^\([a-z]+\)\(-[a-z]*\)?$', `\1'),
                         `', m4_conf_origin_min())')

m4_dnl
m4_dnl Expand to the maximum from an origin range.
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 An origin range
m4_dnl
m4_define(`m4_conf_origin_range_max',
          `m4_repl_cases(m4_regexp(`$1', `^\([a-z]*-\)?\([a-z]+\)$', `\2'),
                         `', m4_conf_origin_max())')

m4_dnl
m4_dnl Expand to "1" if a configuration origin is within a range, and to "0"
m4_dnl otherwise.
m4_dnl
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 An origin to check
m4_dnl      $2 An origin range to check against
m4_dnl
m4_define(`m4_conf_origin_is_in_range',
          `m4_eval(m4_conf_origin_val(`$1') >=
                             m4_conf_origin_val(m4_conf_origin_range_min(`$2')) &&
                             m4_conf_origin_val(`$1') <=
                             m4_conf_origin_val(m4_conf_origin_range_max(`$2')))')

m4_divert(0)m4_dnl
