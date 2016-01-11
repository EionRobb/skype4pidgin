SkypeWeb Plugin for Pidgin
==========================

Adds a "Skype (HTTP)" protocol to the accounts list.  Requires libjson-glib.  GPLv3 Licenced.

Download latest releases from [here](https://github.com/EionRobb/skype4pidgin/releases)

Change Log
----------
  * 1.1 - Support file transfers, fix for Live logins, fix for other crashes
  * 1.0 - Support Live (email address) logins

Windows
-------
Windows users can download latest builds from [here](https://github.com/EionRobb/skype4pidgin/releases)

The plugin requires libjson-glib which is part of the installer exe or can be downloaded [from github](https://github.com/EionRobb/skype4pidgin/raw/master/skypeweb/libjson-glib-1.0.dll) and copied to the Program Files\Pidgin folder (not the plugins subfolder)

Fedora
---------
On Fedora you can use [purple-skypeweb](https://copr.fedoraproject.org/coprs/xvitaly/purple-skypeweb/) COPR repository. [Later](https://bugzilla.redhat.com/show_bug.cgi?id=1294523), after package review, it will be available from main Fedora repository.
At first time you should add COPR repository and enable it:
```
	sudo dnf copr enable xvitaly/purple-skypeweb
```
Now you can install packages:
```
	sudo dnf install purple-skypeweb pidgin-skypeweb
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
	sudo apt install libpurple-dev libjson-glib-dev cmake gcc
	git clone git://github.com/EionRobb/skype4pidgin.git
	cd skype4pidgin/skypeweb
	mkdir build
	cd build
	cmake ..
	cpack
```
To install do:
```
	sudo dpkg -i skypeweb-0.1.0-Linux.deb
```


Building AUR package for Arch Linux
----------
[AUR Package Site](https://aur.archlinux.org/packages/purple-skypeweb)
First you will need to get the [build deps](https://wiki.archlinux.org/index.php/Arch_User_Repository#Prerequisites) unless you already have them
```
# pacman -S --needed base-devel
```
Next, clone the package's [AUR git repo](https://aur.archlinux.org/purple-skypeweb.git)
```
$ git clone https://aur.archlinux.org/purple-skypeweb.git
```
Finally, [build and install](https://wiki.archlinux.org/index.php/Arch_User_Repository#Build_and_install_the_package)
```
$ cd purple-skypeweb
$ makepkg -sri
```

For more information about the Arch User Repository and how installs work, documentation can be found on the [ArchWiki AUR Page](https://wiki.archlinux.org/index.php/AUR)


Contact me
----------
I (Eion) normally hang out on Freenode in #pidgin (irc://chat.freenode.net/pidgin) or you can Skype me (skype:bigbrownchunx) directly or send me [an email](mailto:eionrobb+skype%40gmail.com) or [open an issue](https://github.com/EionRobb/skype4pidgin/issues/new) if you prefer.  I'm always happy to hear from you :)

Show your appreciation
----------------------
Did this plugin make your life happier?  [Send me $1](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=D33N5RV7FEXZU) to say thanks!
