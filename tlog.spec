Name:       tlog
Version:    2
Release:    1%{?dist}
Summary:    Terminal I/O logger

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

%global _hardened_build 1

%prep
%setup -q

%build
%configure --disable-rpath --disable-static
make %{?_smp_mflags}

%check
make %{?_smp_mflags} check

%install
make install DESTDIR=%{buildroot}
rm %{buildroot}/%{_libdir}/*.la
# Remove development files as we're not doing a devel package yet
rm %{buildroot}/%{_libdir}/*.so
rm -r %{buildroot}/usr/include/%{name}

%files
%doc %{_defaultdocdir}/%{name}
%{_bindir}/%{name}-*
%{_libdir}/lib%{name}.so*
%{_datadir}/%{name}
%{_mandir}/man5/*
%{_mandir}/man8/*
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-rec.conf
%config(noreplace) %{_sysconfdir}/%{name}/%{name}-play.conf

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Thu Mar 10 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 2-1
- Release v2. Not ready for production. Following features are added.
- Fully-fledged command-line interface to tlog-play, along with config file
  and man pages.
- Support for playback from file in tlog-play.
- Make tlog-play follow mode controllable and off by default.

* Thu Feb 25 2016 Nikolai Kondrashov <Nikolai.Kondrashov@redhat.com> - 1-1
- Release v1. Not ready for production. Following features are included.
- Recording of user input, program output and window size changes.
- Support for writing into syslog and files.
- Tlog-rec configuration through system-wide configuration file
  /etc/tlog/tlog-rec.conf, environment variables and command line.
- Very basic playback directly from ElasticSearch.
