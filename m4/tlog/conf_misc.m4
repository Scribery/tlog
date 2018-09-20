m4_dnl
m4_dnl Miscellaneous configuration schema handling macros.
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
m4_include(`conf_origin.m4')m4_dnl
m4_divert(-1)

m4_dnl
m4_dnl Increment macro M4_CONF_CONTAINER_SIZE_VAL, if a parameter belongs to
m4_dnl the container specified with M4_CONF_PREFIX and M4_CONF_ORIGIN is in
m4_dnl its range.
m4_dnl Macros:
m4_dnl
m4_dnl  M4_CONF_PREFIX - container prefix (`' for root)
m4_dnl  M4_CONF_ORIGIN - origin to match against
m4_dnl  M4_CONF_CONTAINER_SIZE_VAL - the macro to increment
m4_dnl
m4_define(
`_M4_CONF_CONTAINER_SIZE_ADD_PARAM',
`m4_ifelse(`$1', M4_CONF_PREFIX(),
`m4_ifelse(m4_conf_origin_is_in_range(M4_CONF_ORIGIN(), `$3'), 1,
`m4_define(`M4_CONF_CONTAINER_SIZE_VAL',
m4_incr(M4_CONF_CONTAINER_SIZE_VAL)m4_dnl
)'m4_dnl
)'m4_dnl
)'m4_dnl
)

m4_dnl
m4_dnl Expand to the number of parameters directly under specified container,
m4_dnl which match the specified origin.
m4_dnl Arguments:
m4_dnl
m4_dnl  $1 Path to the schema file
m4_dnl  $2 Container prefix
m4_dnl  $3 Origin to match against
m4_dnl
m4_define(
`M4_CONF_CONTAINER_SIZE',
`m4_pushdef(`M4_CONF_CONTAINER_SIZE_VAL', `0')m4_dnl
m4_pushdef(`M4_CONTAINER', `')m4_dnl
m4_pushdef(`M4_PARAM', m4_defn(`_M4_CONF_CONTAINER_SIZE_ADD_PARAM'))m4_dnl
m4_pushdef(`M4_PARAM_LAST_FIELD', m4_defn(`_M4_CONF_CONTAINER_SIZE_ADD_PARAM'))m4_dnl
m4_pushdef(`M4_CONF_PREFIX', `$2')m4_dnl
m4_pushdef(`M4_CONF_ORIGIN', `$3')m4_dnl
m4_include(`$1')m4_dnl
m4_popdef(`M4_CONF_ORIGIN')m4_dnl
m4_popdef(`M4_CONF_PREFIX')m4_dnl
m4_popdef(`M4_PARAM')m4_dnl
m4_popdef(`M4_PARAM_LAST_FIELD')m4_dnl
m4_popdef(`M4_CONTAINER')m4_dnl
M4_CONF_CONTAINER_SIZE_VAL()m4_dnl
m4_popdef(`M4_CONF_CONTAINER_SIZE_VAL')'m4_dnl
)

m4_divert(0)m4_dnl
