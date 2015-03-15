Name "Pidgin - skypeweb plugin"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText " "
# requires NSIS 3.0+
Unicode true
AutoCloseWindow true

LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"
  
;Version Resource
VIProductVersion "1.0.0.0"
VIAddVersionKey /lang=${LANG_ENGLISH} ProductName "Pidgin - skypeweb plugin"
VIAddVersionKey /lang=${LANG_ENGLISH} CompanyName "Pidgin"
VIAddVersionKey /lang=${LANG_ENGLISH} LegalCopyright "Copyright (c) by Pidgin"
VIAddVersionKey /lang=${LANG_ENGLISH} ProductVersion "1.0.0.0"
VIAddVersionKey /lang=${LANG_ENGLISH} FileVersion "1.0.0.0"
VIAddVersionKey /lang=${LANG_ENGLISH} FileDescription "Pidgin - skypeweb Setup"

OutFile "pidgin-skypeweb.exe"

InstallDir "$PROGRAMFILES\Pidgin"

# default section start
Section

SetOutPath $INSTDIR
SetOverwrite on
  
FindWindow $0 "gdkWindowTempShadow" "Pidgin"
StrCmp $0 0 notRunning
    MessageBox MB_OK|MB_ICONEXCLAMATION "Pidgin is running. Please close it first" /SD IDOK
    Abort
notRunning:

# shared glib
SetOutPath "$INSTDIR\Gtk\bin"
File Pidgin\libjson-glib-1.0.dll
File Pidgin\libintl-8.dll
File Pidgin\libgcc_s_dw2-1.dll
File Pidgin\libiconv-2.dll
File Pidgin\libwinpthread-1.dll

# plugin
SetOutPath "$INSTDIR\plugins"
File Pidgin\plugins\libskypeweb.dll

# icons
SetOutPath "$INSTDIR\pixmaps\pidgin\protocols\16"
File ..\..\icons\16\skype.png
SetOutPath "$INSTDIR\pixmaps\pidgin\protocols\22"
File ..\..\icons\22\skype.png
SetOutPath "$INSTDIR\pixmaps\pidgin\protocols\48"
File ..\..\icons\48\skype.png
SetOutPath "$INSTDIR\pixmaps\pidgin\protocols\scalable"
#File Pidgin\pixmaps\pidgin\protocols\scalable\skype.svg

WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\pidgin-skypeweb" "DisplayName" "Pidgin - skypeweb plugin"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\pidgin-skypeweb" "UninstallString" "$INSTDIR\skypeweb-uninstall.exe"

WriteUninstaller $INSTDIR\skypeweb-uninstall.exe

# Do not run as admin
Exec '"$WINDIR\explorer.exe" "$INSTDIR\Pidgin.exe"'

SectionEnd

Section "Uninstall"

FindWindow $0 "gdkWindowTempShadow" "Pidgin"
StrCmp $0 0 notRunning
    MessageBox MB_OK|MB_ICONEXCLAMATION "Pidgin is running. Please close it first" /SD IDOK
    Abort
notRunning:

# Always delete uninstaller first
Delete $INSTDIR\skypeweb-uninstall.exe

# possibly shared files - do not delete
#Delete $INSTDIR\Gtk\bin\libjson-glib-1.0.dll
#Delete $INSTDIR\Gtk\bin\libintl-8.dll
#Delete $INSTDIR\Gtk\bin\libgcc_s_dw2-1.dll
#Delete $INSTDIR\Gtk\bin\libiconv-2.dll
#Delete $INSTDIR\Gtk\bin\libwinpthread-1.dll

# plugin
Delete $INSTDIR\plugins\libskypeweb.dll

# icons
Delete $INSTDIR\pixmaps\pidgin\protocols\16\skype.png
Delete $INSTDIR\pixmaps\pidgin\protocols\22\skype.png
Delete $INSTDIR\pixmaps\pidgin\protocols\48\skype.png
Delete $INSTDIR\pixmaps\pidgin\protocols\scalable\skype.svg

DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\pidgin-skypeweb"

SectionEnd