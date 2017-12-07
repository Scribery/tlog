/*
 * Tlog timestamp string module test.
 *
 * Copyright (C) 2017 Red Hat
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

#include <tlog/timestr.h>
#include <tlog/timespec.h>
#include <stdio.h>

bool
test(const char *file, int line, const char *timestr,
     bool exp_rc, struct timespec exp_ts)
{
    bool passed;
    bool rc_passed;
    bool ts_passed;
    bool rc;
    struct timespec ts = TLOG_TIMESPEC_ZERO;

    rc = tlog_timestr_to_timespec(timestr, &ts);
    ts_passed = ts.tv_sec == exp_ts.tv_sec && ts.tv_nsec == exp_ts.tv_nsec;
    rc_passed = rc == exp_rc;
    passed = ts_passed && rc_passed;

    if (passed) {
        fprintf(stderr,
                "PASS %s:%d tlog_timestr_to_timespec(\"%s\", "
                TLOG_TIMESPEC_FMT ") = %s\n",
                file, line,
                timestr, TLOG_TIMESPEC_ARG(&ts), (rc ? "true" : "false"));
    } else {
        fprintf(stderr,
                "FAIL %s:%d tlog_timestr_to_timespec(\"%s\", "
                TLOG_TIMESPEC_FMT " %s " TLOG_TIMESPEC_FMT") = "
                "%s %s %s\n",
                file, line,
                timestr, TLOG_TIMESPEC_ARG(&ts), (ts_passed ? "==" : "!="),
                TLOG_TIMESPEC_ARG(&exp_ts),
                (rc ? "true" : "false"), (rc_passed ? "==" : "!="),
                (exp_rc ? "true" : "false"));
    }
    return passed;
}

int
main(void)
{
    bool passed = true;
    char buf[128];

#define TS(_sec, _nsec) (struct timespec){_sec, _nsec}
#define TEST(_timestr, _exp_rc, _exp_ts) \
    passed = test(__FILE__, __LINE__, _timestr, _exp_rc, _exp_ts) && passed;

    TEST("A", false, TS(0, 0));
    TEST(",", false, TS(0, 0));
    TEST("+", false, TS(0, 0));
    TEST("-", false, TS(0, 0));
    TEST("-1", false, TS(0, 0));

    TEST("", true, TS(0, 0));
    TEST(" ", true, TS(0, 0));
    TEST("  ", true, TS(0, 0));
    TEST(".", true, TS(0, 0));
    TEST(":", true, TS(0, 0));
    TEST(":.", true, TS(0, 0));
    TEST("::", true, TS(0, 0));
    TEST("::.", true, TS(0, 0));
    TEST(" ::. ", true, TS(0, 0));
    TEST("  ::.  ", true, TS(0, 0));

    TEST("1", true, TS(1, 0));
    TEST("1.", true, TS(1, 0));
    TEST(":1", true, TS(1, 0));
    TEST("::1", true, TS(1, 0));
    TEST("::1.", true, TS(1, 0));
    TEST("0:1", true, TS(1, 0));
    TEST("0:0:1", true, TS(1, 0));
    TEST("0:0:1.", true, TS(1, 0));
    TEST(".000000001", true, TS(0, 1));
    TEST(":.000000001", true, TS(0, 1));
    TEST("::.000000001", true, TS(0, 1));
    TEST("0.000000001", true, TS(0, 1));
    TEST("0:0.000000001", true, TS(0, 1));
    TEST("0:0:0.000000001", true, TS(0, 1));

    TEST(".1", true, TS(0, 100000000));
    TEST(".100000000", true, TS(0, 100000000));

    TEST(".999999999", true, TS(0, 999999999));
    TEST(".1000000000", false, TS(0, 0));

    TEST("10", true, TS(10, 0));
    TEST("100", true, TS(100, 0));
    TEST("1000", true, TS(1000, 0));
    TEST("10000", true, TS(10000, 0));

    TEST("1:", true, TS(60, 0));
    TEST("10:", true, TS(60*10, 0));
    TEST("1::", true, TS(60*60, 0));
    TEST("10::", true, TS(60*60*10, 0));
    TEST("10:10:10", true, TS(60*60*10 + 60*10 + 10, 0));
    TEST("10:10:10.10", true, TS(60*60*10 + 60*10 + 10, 100000000));

    snprintf(buf, sizeof(buf), "%ld", LONG_MAX);
    TEST(buf, true, TS(LONG_MAX, 0));
    snprintf(buf, sizeof(buf), "%ld:", LONG_MAX);
    TEST(buf, false, TS(0, 0));
    snprintf(buf, sizeof(buf), "%ld:", LONG_MAX / 60);
    TEST(buf, true, TS(LONG_MAX / 60 * 60, 0));
    snprintf(buf, sizeof(buf), "%ld:", LONG_MAX / 60 + 1);
    TEST(buf, false, TS(0, 0));
    snprintf(buf, sizeof(buf), "%ld::", LONG_MAX / (60 * 60));
    TEST(buf, true, TS(LONG_MAX / (60 * 60) * (60 * 60), 0));
    snprintf(buf, sizeof(buf), "%ld::", LONG_MAX / (60 * 60) + 1);
    TEST(buf, false, TS(0, 0));

    TEST("999999999999999999999999999", false, TS(0, 0));

    TEST("000000000000000000000000000", true, TS(0, 0));
    TEST("000000000000000000000000001", true, TS(1, 0));
    TEST("000000000000000000000:0000000000000000", true, TS(0, 0));
    TEST("000000000000000000000000000:000000000000000000000:0000000000000000",
         true, TS(0, 0));

    return !passed;
}
