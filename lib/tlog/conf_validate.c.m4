m4_include(`misc.m4')m4_dnl
m4_include(`conf_origin.m4')m4_dnl
/*
 * Tlog-M4_PROG_NAME() JSON configuration validation.
 *
m4_generated_warning(` * ')m4_dnl
 *
 * Copyright (C) 2016 Red Hat
 *
 * This file is part of tlog.
 *
 * Tlog is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Tlog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tlog; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

`#'include <tlog/M4_PROG_SYM()_conf_validate.h>
#include <tlog/grc.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

m4_divert(-1)

m4_define(`M4_PREFIX', `')

m4_define(
    `M4_TYPE_BOOL',
    `
        m4_printl(
           `            if (type != json_type_boolean) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
           `                                name, json_type_to_name(type));',
           `                return TLOG_RC_FAILURE;',
           `            }')
    '
)

m4_define(
    `M4_TYPE_STRING',
    `
        m4_printl(
           `            if (type != json_type_string) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
           `                                name, json_type_to_name(type));',
           `                return TLOG_RC_FAILURE;',
           `            }')
    '
)

m4_define(
    `M4_TYPE_CHOICE_LIST',
    `
        m4_ifelse(`$#', `0', `',
                  `$#', `1', `m4_print(`"$1"')',
                  `
                    m4_printl(`"$1"`,'')
                    m4_print(`                                        ')
                    M4_TYPE_CHOICE_LIST(m4_shift($@))
                  '
        )
    '
)

m4_define(
    `M4_TYPE_CHOICE',
    `
        m4_printl(
           `            if (type != json_type_string) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
           `                                name, json_type_to_name(type));',
           `                return TLOG_RC_FAILURE;',
           `            }')
        m4_print(
            `            const char *value_list[] = {')
        M4_TYPE_CHOICE_LIST(m4_shift($@))
        m4_printl(
           `};',
           `            const char *value = json_object_get_string(obj);',
           `            size_t i;',
           `            for (i = 0;',
           `                 i < TLOG_ARRAY_SIZE(value_list) &&',
           `                 strcmp(value, value_list[i]) != 0;',
           `                 i++);',
           `            if (i >= TLOG_ARRAY_SIZE(value_list)) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" value: %s",',
           `                                name, value);',
           `                return TLOG_RC_FAILURE;',
           `            }')
    '
)

m4_define(
    `M4_TYPE_INT',
    `
        m4_printl(
           `            if (type != json_type_int) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
           `                                name, json_type_to_name(type));',
           `                return TLOG_RC_FAILURE;',
           `            }')
        m4_ifelse(
            `$1', , ,
            `
                m4_printl(
                   `            int64_t value = json_object_get_int64(obj);',
                   `            if (value < $2) {',
                   `                tlog_errs_pushf(perrs, "Invalid \"%s\" value: %" PRId64,',
                   `                                name, value);',
                   `                return TLOG_RC_FAILURE;',
                   `            }')
            '
        )
    '
)

m4_define(
    `M4_TYPE_DOUBLE',
    `
        m4_printl(
           `            if (type != json_type_double) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
           `                                name, json_type_to_name(type));',
           `                return TLOG_RC_FAILURE;',
           `            }')
        m4_ifelse(
            `$1', , ,
            `
                m4_printl(
                   `            double value = json_object_get_double(obj);',
                   `            if (value < $2) {',
                   `                tlog_errs_pushf(perrs, "Invalid \"%s\" value: %g",',
                   `                                name, value);',
                   `                return TLOG_RC_FAILURE;',
                   `            }')
            '
        )
    '
)

m4_define(
    `M4_TYPE_STRING_ARRAY',
    `
        m4_printl(
           `            if (type != json_type_array) {',
           `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
           `                                name, json_type_to_name(type));',
           `                return TLOG_RC_FAILURE;',
           `            }',
           `            int i;',
           `            for (i = 0; i < json_object_array_length(obj); i++) {',
           `                struct json_object *item_obj = json_object_array_get_idx(obj, i);',
           `                enum json_type item_type = json_object_get_type(item_obj);',
           `                if (item_type != json_type_string) {',
           `                    tlog_errs_pushf(perrs, "Invalid \"%s\" item #%d type: %s",',
           `                                    name, i, json_type_to_name(item_type));',
           `                    return TLOG_RC_FAILURE;',
           `                }',
           `            }')

    '
)

m4_define(
    `M4_CONTAINER_VALIDATE_PARAM',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_printl(
                   `',
                   `        if (origin >= 'm4_conf_origin_sym(m4_conf_origin_range_min(`$3'))` &&',
                   `            strcmp(name, "$2") == 0) {')
                $4
                m4_printl(
                   `            continue;',
                   `        }')
            '
        )
    '
)

m4_define(
    `M4_CONTAINER_VALIDATE_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_printl(
                   `',
                   `        if (strcmp(name, "m4_substr(`$2', 1)") == 0) {',
                   `            tlog_grc grc;',
                   `            if (type != json_type_object) {',
                   `                tlog_errs_pushf(perrs, "Invalid \"%s\" type: %s",',
                   `                                name, json_type_to_name(type));',
                   `                return TLOG_RC_FAILURE;',
                   `            }',
                   `            grc = tlog_'M4_PROG_SYM()`_conf_validate`'m4_translit(`$1$2', `/', `_')`'(perrs, obj, origin);',
                   `            if (grc != TLOG_RC_OK) {',
                   `                tlog_errs_pushf(perrs, "Failed to validate \"%s\" section", name);',
                   `                return grc;',
                   `            }',
                   `            continue;',
                   `        }')
            '
        )
    '
)

m4_define(
    `M4_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_pushdef(`M4_PREFIX', M4_PREFIX()`$2')

                m4_pushdef(`M4_PARAM')
                m4_include(M4_PROG_SYM()`_conf_schema.m4')
                m4_popdef(`M4_PARAM')

                m4_pushdef(`M4_CONTAINER', m4_defn(`M4_CONTAINER_VALIDATE_CONTAINER'))
                m4_pushdef(`M4_PARAM', m4_defn(`M4_CONTAINER_VALIDATE_PARAM'))

                m4_ifelse(`$2', `', , `m4_print(`static ')')
                m4_printl(
                   `tlog_grc',
                   `tlog_'M4_PROG_SYM()`_conf_validate`'m4_translit(M4_PREFIX(), `/', `_')(struct tlog_errs **perrs,',
                   `                                struct json_object *conf,',
                   `                                enum tlog_conf_origin origin)',
                   `{',
                   `    assert(conf != NULL);',
                   `',
                   `    json_object_object_foreach(conf, name, obj) {',
                   `        enum json_type type = json_object_get_type(obj);')
                m4_include(M4_PROG_SYM()`_conf_schema.m4')
                m4_printl(
                    `',
                    `        tlog_errs_pushf(perrs, "Unexpected node: \"%s\"", name);',
                    `        return TLOG_RC_FAILURE;',
                    `    }',
                    `    return TLOG_RC_OK;',
                    `}',
                    `')

                m4_popdef(`M4_PARAM')m4_dnl
                m4_popdef(`M4_CONTAINER')m4_dnl

                m4_popdef(`M4_PREFIX')
            '
        )
    '
)

M4_CONTAINER(`', `', `')
