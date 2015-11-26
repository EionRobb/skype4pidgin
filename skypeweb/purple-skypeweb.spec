%global debug_package %{nil}
%global plugin_name skypeweb
%global project_name skype4pidgin
%global purplelib_name purple-%{plugin_name}

Name: %{project_name}
Version: 1.0
Release: 1%{?dist}
Summary: Skype plugin for Pidgin/Adium/libpurple
Group: Applications/Productivity
License: GPLv3
URL: https://github.com/EionRobb/skype4pidgin
Source0: %{project_name}-%{version}.tar.gz
Requires: pidgin-%{plugin_name}

%description
Skype for Pidgin meta package.

%package -n %{purplelib_name}
Summary: Adds support for Skype to Pidgin
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(purple)
BuildRequires: pkgconfig(json-glib-1.0)
BuildRequires: gcc

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Skype protocol.
Requires: %{purplelib_name}
Requires: pidgin

%description -n %{purplelib_name}
Adds support for Skype to Pidgin, Adium, Finch and other libpurple 
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Skype protocol inplemented by libskypeweb.

%prep -n %{purplelib_name}
%setup -c
perl -i -pe 's/\r\n/\n/gs' */%{plugin_name}/README.md

%build -n %{purplelib_name}
cd %{project_name}-*/%{plugin_name}
%make_build

%install
cd %{project_name}-*/%{plugin_name}
%make_install

%files -n %{purplelib_name}
%{_libdir}/purple-2/lib%{plugin_name}.so
%doc */%{plugin_name}/README.md */CHANGELOG.txt
%license */COPYING.txt

%files -n pidgin-%{plugin_name}
%dir %{_datadir}/pixmaps/pidgin
%dir %{_datadir}/pixmaps/pidgin/protocols
%dir %{_datadir}/pixmaps/pidgin/protocols/16
%{_datadir}/pixmaps/pidgin/protocols/16/skype.png
%{_datadir}/pixmaps/pidgin/protocols/16/skypeout.png
%dir %{_datadir}/pixmaps/pidgin/protocols/22
%{_datadir}/pixmaps/pidgin/protocols/22/skype.png
%{_datadir}/pixmaps/pidgin/protocols/22/skypeout.png
%dir %{_datadir}/pixmaps/pidgin/protocols/48
%{_datadir}/pixmaps/pidgin/protocols/48/skype.png
%{_datadir}/pixmaps/pidgin/protocols/48/skypeout.png
%dir %{_datadir}/pixmaps/pidgin/emotes
%dir %{_datadir}/pixmaps/pidgin/emotes/skype
%{_datadir}/pixmaps/pidgin/emotes/skype/theme

%files

%changelog
* Thu Nov 26 2015 V1TSK <vitaly@easycoding.org> - 1.0-2
- Applyed Maxim Orlov's fixes.

* Sun Nov 08 2015 V1TSK <vitaly@easycoding.org> - 1.0-1
- Updated to version 1.0.

* Mon Aug 24 2015 jparvela <jparvela@gmail.com> - 0.1-4
- Added missing files to spec file list.

* Mon Aug 03 2015 BOPOHA <vorona.tolik@gmail.com> - 0.1-3
- Fixed build with OBS. RPMS can be built from main tarball.

* Sat May 09 2015 V1TSK <vitaly@easycoding.org> - 0.1-2
- Separated packages. Now can be used with other libpurple-based clients without Pidgin being installed.

* Mon Mar 16 2015 V1TSK <vitaly@easycoding.org> - 0.1-1
- Created first RPM spec for Fedora/openSUSE.
