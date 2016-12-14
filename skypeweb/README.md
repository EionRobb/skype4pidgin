SkypeWeb Plugin for Pidgin
==========================

Adds a "Skype (HTTP)" protocol to the accounts list.  Requires libjson-glib.  GPLv3 Licenced.

Download latest releases from [here](https://github.com/EionRobb/skype4pidgin/releases)

Change Log
----------
  * 1.2.2 - Fixes non-Live logins
  * 1.2.1 - Fixes support for Live logins again, fixes setting mood message setting, fixes friend search, hopefully less disconnects
  * 1.2 - Adds support for libpurple 2.11.0, allows setting "mood" messages
  * 1.1 - Support file transfers, fix for Live logins, fix for other crashes
  * 1.0 - Support Live (email address) logins
  
Issues
------
Before posting an issue, please check ![Error message flow chart](http://dequis.org/skypeweb.png)

Windows
-------
Windows users can download latest builds from [here](https://github.com/EionRobb/skype4pidgin/releases)

The plugin requires libjson-glib which is part of the installer exe or can be downloaded [from github](https://github.com/EionRobb/skype4pidgin/raw/master/skypeweb/libjson-glib-1.0.dll) and copied to the Program Files\Pidgin folder (not the plugins subfolder)

Fedora
---------
On Fedora you can install [package](https://apps.fedoraproject.org/packages/purple-skypeweb) from Fedora's main repository:
```
	sudo dnf install purple-skypeweb pidgin-skypeweb
```

CentOS/RHEL
---------
On CentOS/RHEL you can install [package](https://apps.fedoraproject.org/packages/purple-skypeweb) from Fedora's [EPEL7](http://fedoraproject.org/wiki/EPEL) repository:

```
	sudo yum install purple-skypeweb pidgin-skypeweb
```

Arch Linux
----------
On Arch Linux package available in [Community](https://wiki.archlinux.org/index.php/official_repositories#community) repository. Installation is usual:
```
	sudo pacman -S purple-skypeweb
```

openSUSE
--------
On openSUSE you can install the [package](https://software.opensuse.org/package/pidgin-plugin-skypeweb) using
```
	sudo zypper in pidgin-plugin-skypeweb
```

Compiling
---------
Requires devel headers/libs for libpurple and libjson-glib [libglib2.0-dev, libjson-glib-dev and libpurple-dev]
```	
	git clone git://github.com/EionRobb/skype4pidgin.git
	cd skype4pidgin/skypeweb
	make
	sudo make install
```

Building RPM package for Fedora/openSUSE/CentOS/RHEL
---------
Requires devel headers/libs for libpurple and json-glib, gcc compiler and rpmbuild tool
```
	sudo yum install rpm-build gcc json-glib-devel libpurple-devel zlib-devel make automake glib2-devel spectool -y
	mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
	wget https://github.com/EionRobb/skype4pidgin/raw/master/skypeweb/purple-skypeweb.spec -O ~/rpmbuild/SPECS/purple-skypeweb.spec
	spectool --all --get-files ~/rpmbuild/SPECS/purple-skypeweb.spec --directory ~/rpmbuild/SOURCES/
	rpmbuild -ba  ~/rpmbuild/SPECS/purple-skypeweb.spec
```
The result can be found in ``~/rpmbuild/RPMS/`uname -m`/`` directory.


Building DEB package for Debian/Ubuntu/Mint
---------
Requires devel headers/libs for libpurple and json-glib, gcc compiler and cmake
```
	sudo apt-get install libpurple-dev libjson-glib-dev cmake gcc
	git clone git://github.com/EionRobb/skype4pidgin.git
	cd skype4pidgin/skypeweb
	mkdir build
	cd build
	cmake ..
	cpack
```
To install do:
```
	sudo dpkg -i skypeweb-1.2.2-Linux.deb
```

Building AUR package for Arch Linux
----------
[AUR Package Site](https://aur.archlinux.org/packages/purple-skypeweb-git)
First you will need to get the [build deps](https://wiki.archlinux.org/index.php/Arch_User_Repository#Prerequisites) unless you already have them
```
	sudo pacman -S --needed base-devel
```
Next, clone the package's [AUR git repo](https://aur.archlinux.org/purple-skypeweb-git.git)
```
	git clone https://aur.archlinux.org/purple-skypeweb-git.git
```
Finally, [build and install](https://wiki.archlinux.org/index.php/Arch_User_Repository#Build_and_install_the_package)
```
	cd purple-skypeweb-git
	makepkg -sri
```

For more information about the Arch User Repository and how installs work, documentation can be found on the [ArchWiki AUR Page](https://wiki.archlinux.org/index.php/AUR)

Adium
-----
The magical [tripplet](https://github.com/tripplet) (who ported the [Steam prpl to Adium](https://github.com/tripplet/Adium-Steam-IM)) has ported [SkypeWeb to Adium](https://github.com/tripplet/skypeweb4adium) too.  Releases for this are at https://github.com/tripplet/skypeweb4adium/releases


Contact me
----------
I (Eion) normally hang out on Freenode in #pidgin (irc://chat.freenode.net/pidgin) or you can Skype me (skype:bigbrownchunx) directly or send me [an email](mailto:eionrobb+skype%40gmail.com) or [open an issue](https://github.com/EionRobb/skype4pidgin/issues/new) if you prefer.  I'm always happy to hear from you :)

Show your appreciation
----------------------
Did this plugin make your life happier?  [Send me $1](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=D33N5RV7FEXZU) to say thanks!
