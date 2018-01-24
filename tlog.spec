Name:       tlog
Version:    5
Release:    1%{?dist}
Summary:    Terminal I/O logger
Group:      Applications/System

License:    GPLv2+
URL:        https://github.com/Scribery/%{name}
Source:     https://github.com/Scribery/%{name}/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  json-c-devel
BuildRequires:  curl-devel
BuildRequires:  m4
# If it's not RHEL6 and older
%if 0%{?rhel} == 0 || 0%{?rhel} >= 7
BuildRequires:  systemd-devel
BuildRequires:  systemd-units
%endif
Requires(post):     sed
Requires(postun):   sed

BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
Tlog is a terminal I/O recording program similar to "script", but used in
place of a user's shell, starting the recording and executing the real user's
shell afterwards. The recorded I/O can then be forwarded to a logging server
in JSON format.

%global _hardened_build 1

%prep
%setup -q

%build
# If it's not RHEL6 and older
%if 0%{?rhel} == 0 || 0%{?rhel} >= 7
%configure --disable-rpath --disable-static
# Else, if it's RHEL6 or older
%else
%configure --disable-rpath --disable-static --disable-journal
%endif
make %{?_smp_mflags}

%check
make %{?_smp_mflags} check

%pre
getent group %{name} >/dev/null ||
    groupadd -r %{name}
getent passwd %{name} >/dev/null ||
    useradd -r -g %{name} -d %{_localstatedir}/run/%{name} -s /sbin/nologin \
            -c "Tlog terminal I/O logger" %{name}

%install
make install DESTDIR=%{buildroot}
rm %{buildroot}/%{_libdir}/*.la
# Remove development files as we're not doing a devel package yet
rm %{buildroot}/%{_libdir}/*.so
rm -r %{buildroot}/usr/include/%{name}

# If it's not RHEL6 and older
%if 0%{?rhel} == 0 || 0%{?rhel} >= 7
    # Create tmpfiles.d configuration for the lock dir
    mkdir -p %{buildroot}%{_tmpfilesdir}
    {
        echo "# Type Path Mode UID GID Age Argument"
        echo "d /run/%{name} 0755 %{name} %{name}"
    } > %{buildroot}%{_tmpfilesdir}/%{name}.conf
    # Create the lock dir
    mkdir -p %{buildroot}/run
    install -d -m 0755 %{buildroot}/run/%{name}
# Else, if it's RHEL6 or older
%else
    # Create the lock dir
    mkdir -p %{buildroot}%{_localstatedir}/run
    install -d -m 0755 %{buildroot}%{_localstatedir}/run/%{name}
%endif

%files
%{!?_licensedir:%global license %doc}
%license COPYING
%doc %{_defaultdocdir}/%{name}
%{_bindir}/%{name}-rec
%attr(6755,%{name},%{name}) %{_bindir}/%{name}-rec-session
%{_bindir}/%{name}-play
%{_libdir}/lib%{name}.so*
%{_datadir}/%{name}
%{_mandir}/man5/*
%{_mandir}/man8/*
# If it's not RHEL6 and older
%if 0%{?rhel} == 0 || 0%{?rhel} >= 7
%config(noreplace) %{_tmpfilesdir}/%{name}.conf
%dir %attr(-,%{name},%{name}) /run/%{name}
# Else if it's RHEL6 or older
%else
%dir %attr(-,%{name},%{name}) %{_localstatedir}/run/%{name}
%endif
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-rec.conf
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-rec-session.conf
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-play.conf

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%changelog
* Wed Jan 24 2018 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 5-1
- Release v5

* Wed Jan 24 2018 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 4-1
- Release v4. Added features and implemented fixes follow. See README.md and
  manpages for documentation of new features.
- Extract user session recording functionality from tlog-rec into a new tool:
  tlog-rec-session. It should be used as the user's login shell now, and
  tlog-rec should be used as a general recording and testing tool.
- Add (optional) support for writing to and reading from Systemd Journal - the
  "journal" reader and writer.
- Make tlog-rec default to "file" writer, and tlog-rec-session to "journal",
  if built with Journal support, and to "syslog" otherwise.
- Add "-o" option to tlog-rec as an alias to "--file-path".
- Add "-i" option to tlog-play as an alias to "--file-path".
- Assume locale charset is UTF-8, if ASCII charset is detected, since that is
  a likely indication the locale settings were lost. E.g. upon console login
  or "su -" on Fedora and RHEL.
- Switch the "ver" JSON field type to string. Now it should be two numbers
  separated by a dot. The increase of the first number indicates
  forward-incompatible changes, the increase of the second number -
  forward-compatible. If the dot and the second number are omitted, the second
  number is considered to be zero. Bump the format version to "2".
- Add a new JSON field: "rec", containing an opaque host-unique recording ID.
  Bump the format version to "2.1".
- Add support for playback controls, both through the command line and via
  playback-time control keys, including: speed adjustment, pause/resume,
  fast-forward to a time, and packet-by-packet stepping through the recording.
- Add optional rate-limiting of logged messages. Both throttling and dropping
  messages are supported.
- Add "--lax" option to tlog-play to allow playing back recordings with
  missing messages.
- Fix input being ignored when there is a lot of output, while recording.
- Remove addition of tlog-rec (tlog-rec-session) to /etc/shells from RPM
  packaging to prevent users from changing their shells themselves once it has
  been assigned.
- Add support for specifying the shell to start via the tlog-rec-session
  executable name. E.g. by making a tlog-rec-session-shell-bin-zsh ->
  tlog-rec-session symlink and executing it. That can be used to specify
  particular shells to be recorded for specific users by assigning these
  symlinks as their login shells.
- Make error messages from all the tools a bit less noisy and more readable.

* Tue Apr 12 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 3-1
- Release v3. Added features and implemented fixes follow.
- Make each JSON message timing data start with window size.
  This makes it possible to pick up the stream from any message and also
  combine messages, with window size known and preserved at all times.
- Add "term" field to JSON messages, specifying terminal type.
- Add "ver" field to JSON messages, specifying message format version.
- Set "SHELL" environment variable to actual user shell in tlog-rec.
- Check for locale's charset and abort tlog-rec if it's anything but the only
  supported UTF-8.
- Add -v/--version option support to tlog-rec and tlog-play.
- Fix tlog-rec and tlog-play error output by accumulating error messages and
  outputting them only after terminal settings are restored, on exit. Output
  startup warnings before switching to raw terminal settings.
- Output a newline after restoring terminal settings in tlog-rec and
  tlog-play, so that following output is not stuck to the end of the last line
  of the raw output.
- Add an Elasticsearch mapping to documentation directory.
- Disable input logging by default to avoid storing passwords. Please enable
  it explicitly in configuration, or on the command line, if necessary.
- Close log file written by tlog-rec on executing the shell in the child to
  prevent log modification by the recorded user.
- Support running tlog-rec SUID/SGID to prevent recorded users from killing or
  modifying it. Make tlog-rec SUID/SGID to user "tlog" in the RPM package.
- Add session locking to tlog-rec. This prevents tlog-rec from recording if
  the audit session is already recorded by creating per-audit-session lock
  files in /var/run/tlog. This only makes sense with tlog-rec SUID/SGID.
  When certain failures occur while creating a lock file, session is assumed
  unlocked and is recorded anyway, as it is safer to record a session than
  not. Add corresponding setup to the RPM package.
- Reproduce the recorded program (shell) exit status in tlog-rec similarly to
  how Bash reproduces the last executed command status.
- Update and expand README.md to describe secure log message filtering with
  rsyslog, and playback directly from Elasticsearch, among other, smaller
  additions.

* Wed Apr 6 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 2-1
- Release v2. Not ready for production. Following features are added.
- Fully-fledged command-line interface to tlog-play, along with config file
  and man pages.
- Support for playback from file in tlog-play.
- Make tlog-play follow mode controllable and off by default.
- Get tlog-rec shell also from TLOG_REC_SHELL environment variable.
- Support non-TTY stdin/stdout in tlog-rec, allowing its use with
  non-interactive SSH sessions.
- Support building on and packaging for EPEL5.

* Thu Feb 25 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 1-1
- Release v1. Not ready for production. Following features are included.
- Recording of user input, program output and window size changes.
- Support for writing into syslog and files.
- Tlog-rec configuration through system-wide configuration file
  /etc/tlog/tlog-rec.conf, environment variables and command line.
- Very basic playback directly from ElasticSearch.
