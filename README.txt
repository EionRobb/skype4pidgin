Skype plugin for Pidgin/Adium/libpurple
=======================================

If you've found this file then you'll probably realise that this is the Skype plugin for libpurple.  Yes, it does require Skype to be running as it uses the Skype API to communicate with a running copy of Skype.  The Windows and Linux versions were made by me in a week.  The OS X version is ongoing and may need reverse engineering of the Skype.framework.


History/Ramble
==============

It seems people have wanted a Skype plugin for Gaim/Pidgin/Adium for a long time, however it seems to be rare to find a glib coder who uses Pidgin who also uses Skype.  There seems to have been a lot of stigma over Skype being closed-source and evil, however there has still been a demand for this plugin and the end-user has suffered for long enough.  That was kind of my motivation.  Plus I enjoy both Pidgin and Skype and it made sense to me to combine the two.  To this day my partner does not see the point of having this plugin if Skype still needs to be running, however something is better than nothing right? :)


Installation
============

For the binary version of the plugin, copy libskype to the appropriate plugins directory.  On Windows its normally C:\Program Files\Pidgin\plugins, on Linux it's normally /usr/lib/purple-2/ or ~/.purple/plugins
To compile from source, the easiest way is extract libskype.zip into the ${PIDGIN_SOURCE}/libpurple/plugins/ directory and compile using "make libpurple.so" or "make -f Makefile.mingw libpurple.dll"


Known Issues
============

* The status/mood text doesn't update until you open a chat window or click on the buddy.
* No multi-user chats yet.
* No notification popup on file being received/notificaitons (Windows)
* Skype doesn't automatically hide again after dealing with notifiations (Windows)
* Skype doesn't logout when pidgin goes offline
* Skype sounds still play even when it's hidden
* The plugin loads in SkypeOut contacts
* The plugin hangs if Skype is running but not logged in
* Adium sometimes crashes on startup when the plugin is loaded.  Sometimes. (Adium)
* Changing status on Linux/OSX doesn't update in Skype. (API Limitation)
* Send file doesn't use the built in libpurple methods. (API Limitation)
* Skype on Linux still opens up Skype message windows. (API Limitation)
* Skype on Linux doesn't show buddy icons. (API Limitation)
* No typing notifications. (API Limitation)


FAQ
===

Q: Does this plugin require Skype to be running?
A: Yes.

Q: Can you make a version that doesn't need Skype?
A: Not possible right now.

Q: Where can I get the protocol icons?
A: https://developer.skype.com/Download/SkypeIcons.  Install the SkypeBlue icons into the appropriate pixmaps/pidgin/protocols directory.

Q: How can I hide the Skype icon from the tray?
A: I'm planning to automate and make this cross-platform.

Q: How can I hide the Skype icon from the tray on Windows?
A: Right click on the taskbar, click Properties, click the Customize... button, find Skype in the list, click on it, choose 'Always Hide' from the drop-down box.

Q: How can I hide Skype in OSX?
A: Focus (click on) the Skype window, press ⌘+H to hide the application.  Alternativly, focus the Skype window and choose 'Hide Skype' from the 'Skype' menu.

Q: How can I hide Skype in Linux?
A: A bit more tricky.  What works well for me, is leaving the message window open, but moving it to another workspace so that it doesn't bother you any more.

Q: The Windows version has feature "X", but the Linux/OSX version doesn't
A: The API on Linux/OSX is more restrictive than the Windows API.  Try keeping up to date with the latest versions of Skype to take advantage of these features as they are implemented by the Skype developers.


Contact
=======

If you've got any questions/problems/comments with this plugin, feel free to flick me an email at ｅｉｏｎ@ｒｏｂｂｍｏｂ.ｃｏｍ or chat message on Skype at bigbrownchunx.


Legal
=====
Although none of this is necessary in my country, it may apply in yours.  New Zealand law permits reverse-engineering for purposes of interoperability, so the following legal notices are just a courtesy.

The licence for the Skype provided icons don't allow them to be distributed in an open-source program without including the entire 2.7MiB package.  I will investigate making my own icon instead.

Skype API Terms of Use:
The following statement must be displayed in the documentation of this appliction:
This plugin "uses Skype Software" to display contacts, and chat to Skype users from within Pidgin
"This product uses the Skype API but is not endorsed, certified or otherwise approved in any way by Skype"

The use of this plugin requries your acceptance of the Skype EULA (http://www.skype.com/intl/en/company/legal/eula/index.html)

Skype is the trademark of Skype Limited

