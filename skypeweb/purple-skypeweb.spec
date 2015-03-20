%define debug_package %{nil}

Name: purple-skypeweb
Version: 0.1
Release: 1
Summary: Adds support for Skype to Pidgin
Group: Applications/Productivity
License: GPLv3
URL: https://github.com/EionRobb/skype4pidgin
Source0: %{name}-%{version}.tar.gz

BuildRequires: glib2-devel
BuildRequires: libpurple-devel
BuildRequires: json-glib-devel
BuildRequires: gcc
Requires: libpurple
Requires: pidgin
Requires: json-glib

%description
Adds support for Skype to Pidgin, Adium, Finch and other libpurple 
based messengers.

%prep
%setup -c

%build
make

%install
make install DESTDIR=%{buildroot}

%files
%{_libdir}/purple-2/libskypeweb.so
%{_datadir}/pixmaps/pidgin/protocols/16/skype.png
%{_datadir}/pixmaps/pidgin/protocols/16/skypeout.png
%{_datadir}/pixmaps/pidgin/protocols/22/skype.png
%{_datadir}/pixmaps/pidgin/protocols/22/skypeout.png
%{_datadir}/pixmaps/pidgin/protocols/48/skype.png
%{_datadir}/pixmaps/pidgin/protocols/48/skypeout.png

%changelog
* Mon Mar 16 2015 V1TSK <vitaly@easycoding.org> - 0.1
- Created first RPM spec for Fedora/openSUSE.
