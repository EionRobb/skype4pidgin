Skype plugin for Pidgin/Adium/libpurple
=======================================

If you've found this file then you'll probably realise that this is the Skype plugin for libpurple.  Yes, it does require Skype to be running as it uses the Skype API to communicate with a running copy of Skype.  The Windows and Linux versions were made by me in a week.  The OS X version has taken lots longer, but it needed reverse engineering of the Skype.framework before it was GPL compatable.


History/Ramble
==============

It seems people have wanted a Skype plugin for Gaim/Pidgin/Adium for a long time, however it seems to be rare to find a glib coder who uses Pidgin who also uses Skype.  There seems to have been a lot of stigma over Skype being closed-source and evil, however there has still been a demand for this plugin and the end-user has suffered for long enough.  That was kind of my motivation.  Plus I enjoy both Pidgin and Skype and it made sense to me to combine the two.  I know it's not the ideal solution (to still have Skype running), but if you're going to have it running, you might as well have your buddies in one place, right?


Installation
============

For the binary version of the plugin, either install the appropriate package or .exe for your distro or copy libskype to the appropriate plugins directory.  On Windows its normally C:\Program Files\Pidgin\plugins, on Linux it's normally /usr/lib/purple-2/ or ~/.purple/plugins
On OSX, double click on SkypePlugin.AdiumPlugin to install (may need a restart of Adium).  You may need to delete the old version which is at ~/Library/Application Support/Adium 2.0/Plugins.  If upgrading to Adium 1.2, you will need to delete the account and re-add it.

To install swanky looking icons, extract skype_icons.zip to the pixmaps/pidgin/protocols directory (on Windows, C:\Program Files\Pidgin\pixmaps\pidgin\protocols).
To have the right smilies/emotes in your Skype chat messages, copy themes into the pixmaps/pidgin/emotes/default folder.

To compile from source, the easiest way is extract libskype.zip into the ${PIDGIN_SOURCE}/libpurple/plugins/ directory and compile using "make libpurple.so" or "make -f Makefile.mingw libpurple.dll"


Known Issues
============

* No notification popup on file being received/notificaitons (Windows)

* Skype sounds still play even when it's hidden

* Adium doesn't show file transfer progress (OS X)
* Unable to send file from within Adium (API Limitation)
* Send file doesn't use the built in libpurple methods. (API Limitation)
* Linux version still opens up Skype message windows. (API Limitation)
* No typing notifications with official Skype clients. (API Limitation)


FAQ
===

Q: Does this plugin require Skype to be running?
A: Yes.

Q: Can you make a version that doesn't need Skype?
A: Not possible right now.

Q: What version of Pidgin do I need to be able to run this plugin?
A: You need at least version 2.1.1 of Pidgin.  You can download updates to Pidgin from www.pidgin.im

Q: What version of Adium do I need to be able to run this plugin?
A: You need version 1.2 to run the latest version of the plugin or 1.1 to run the older version of the plugin.  You can download updates to Adium from www.adiumx.com

Q: Where can I get the protocol icons?
A: Download skype_icons.zip from this website and install the icons into the appropriate pixmaps/pidgin/protocols directory.

Q: How can I hide the Skype icon from the tray?
A: I'm planning to automate and make this cross-platform.

Q: How can I hide the Skype icon from the tray on Windows?
A: Right click on the taskbar, click Properties, click the Customize... button, find Skype in the list, click on it, choose 'Always Hide' from the drop-down box.

Q: How can I hide Skype in OSX?
A: Focus (click on) the Skype window, press ⌘+H to hide the application.  Alternativly, focus the Skype window and choose 'Hide Skype' from the 'Skype' menu.

Q: How can I hide Skype in Linux?
A: A bit more tricky.  What works well for me, is turning off message windows in the Notifications settings in Skype (Options->Chat->Display an event notification).  Otherwise, you can leave the message window open, but move it to another workspace so that it doesn't bother you any more.

Q: The Windows version has feature "X", but the Linux/OSX version doesn't
A: The API on Linux/OSX is more restrictive than the Windows API.  Try keeping up to date with the latest versions of Skype to take advantage of these features as they are implemented by the Skype developers.

Q: Why is "Send file..." not in the conversation menu, but in the "More..." menu instead?
A: The short answer is that Skype doesn't allow file sending in the way that libpurple wants, so it has to do it through a Skype popup instead of a Pidgin popup

Q: I set my status to "Away" but Skype still shows me as "Online"?
A: For some reason the Skype tray icon isn't always what is being seen at the other end.  Just ignore it :)

Q: On OSX, I get bugged with the "Allow API Access" window from Skype every time Adium starts?
A: Turn on "access for assistive devices" See: http://images.apple.com/applescript/uiscripting/gfx/gui.03.jpg

Contact
=======

If you've got any questions/problems/comments with this plugin, feel free to flick me an email at ｅｉｏｎ@ｒｏｂｂｍｏｂ.ｃｏｍ or chat message on Skype at bigbrownchunx.


Legal
=====
Skype is the trademark of Skype Limited

