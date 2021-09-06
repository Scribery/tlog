# Place this script in /etc/profile.d to automatically hook any login or
# interactive shell into tlog for a user or group listed in
# /etc/security/tlog.users
#
# Entries in tlog.users should be listed one per line where users are bare
# words such as `root` and groups are prefixed with a percent sign such as
# `%root`.
#
# Copyright 2018 Trevor Vaughan <tvaughan@onyxpoint.com> - Onyx Point, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
set TLOG_USERS="/etc/security/tlog.users"
set TLOG_CMD="/usr/bin/tlog-rec-session"

if ( -f "$TLOG_USERS" ) then
  if ( ! ($?TLOG_RUNNING) ) then

    set TLOG_D='$'
    set TLOG_PATTERN="^(%$GROUP|$USER)$TLOG_D"
    set TLOG_MATCH=`grep -E "$TLOG_PATTERN" "$TLOG_USERS"`

    if ( "$TLOG_MATCH" != "" ) then
      setenv TLOG_RUNNING true

      setenv TLOG_REC_SESSION_SHELL $SHELL

      if ($?prompt || $?loginsh) then
        set TLOG_CMD="$TLOG_CMD -l"
      endif

      set TLOG_PATTERN='-c[[:space:]]\+.\+'
      set TLOG_PASSTHROUGH_CMD=`ps --no-headers -o args $$ | grep -oe "$TLOG_PATTERN"`

      if ( "$TLOG_PASSTHROUGH_CMD" == "" ) then
        exec $TLOG_CMD
      endif
    endif
  endif
endif
