Name:       tlog
Version:    1
Release:    1%{?dist}
Summary:    Terminal I/O recording shell wrapper

License:    GPLv2+
URL:        https://github.com/Scribery/tlog
Source:     %{name}-%{version}.tar.gz

BuildRequires:  json-c-devel
BuildRequires:  libcurl-devel
BuildRequires:  m4

%description
 Tlog is a terminal I/O recording program similar to "script", but used in
 place of a user's shell, starting the recording and executing the real user's
 shell afterwards. The recorded I/O can then be forwarded to a logging server
 in JSON format.

%prep
%setup -q

%build
%configure --disable-rpath --disable-static
make %{?_smp_mflags}

%check
make %{?_smp_mflags} check

%install
make install DESTDIR=%{buildroot}
rm ${RPM_BUILD_ROOT}/%{_libdir}/*.la
# Remove development files as we're not doing a devel package yet
rm ${RPM_BUILD_ROOT}/%{_libdir}/*.so
rm -r $RPM_BUILD_ROOT/usr/include/%{name}

%files
%doc
%{_defaultdocdir}/%{name}
%{_bindir}/%{name}-*
%{_libdir}/lib%{name}.so*
%{_datadir}/%{name}
%{_mandir}/man5/*
%{_mandir}/man8/*
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-rec.conf
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-play.conf

%changelog
* Wed Mar 09 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 2-1
- Release v2. Not ready for production. Following features are added.
- Add fully-fledged command-line interface to tlog-play, along with config
  file and man pages.

* Thu Feb 25 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 1-1
- Release v1. Not ready for production. Following features are included.
- Recording of user input, program output and window size changes.
- Support for writing into syslog and files.
- Tlog-rec configuration through system-wide configuration file
  /etc/tlog/tlog-rec.conf, environment variables and command line.
- Very basic playback directly from ElasticSearch.
