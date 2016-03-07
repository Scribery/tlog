Name:       tlog
Version:    1
Release:    1%{?dist}
Summary:    Terminal I/O recording shell wrapper

License:    GPLv2+
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

%files
%doc
%{_defaultdocdir}/%{name}
%{_bindir}/tlog-*
%{_includedir}/tlog
%{_libdir}/libtlog.so*
%{_datadir}/tlog
%{_mandir}/man5/*
%{_mandir}/man8/*
%config(noreplace) %{_sysconfdir}/tlog/tlog-rec.conf
%config(noreplace) %{_sysconfdir}/tlog/tlog-play.conf

%changelog
