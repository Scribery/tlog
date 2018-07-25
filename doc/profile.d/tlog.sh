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
TLOG_USERS="/etc/security/tlog.users"
TLOG_CMD="/usr/bin/tlog-rec-session"

tlog_parent(){
  retval=1

  ppid=`ps --no-headers -o ppid $1`

  if [ $ppid -gt 1 ]; then
    if `ps --no-headers -o ppid,args $1 | grep -q 'tlog-rec-session'`; then
      return 0
    else
      tlog_parent $ppid
      retval=$?
    fi

  fi

  return $retval
}

if [ -f "${TLOG_USERS}" ]; then
  if ! `tlog_parent $PPID`; then
    if `grep -qE "^(%${GROUP}|${USER})$" "${TLOG_USERS}"`; then
      if [[ $- == *i* ]] || `shopt -q login_shell`; then
        TLOG_CMD="${TLOG_CMD} -l"
      fi

      if ! `ps --no-headers -o args $$ | grep -qe "-c[[:space:]]\+.\+"`; then
        TLOG_REC_SESSION_SHELL=$SHELL

        exec $TLOG_CMD
      fi
    fi
  fi
fi
