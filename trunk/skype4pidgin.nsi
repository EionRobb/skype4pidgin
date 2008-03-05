; Script based on generated HM NIS Edit Script Wizard.
; Forgive me, i am new at this. -- {paul,ian}@cypherpunks.ca
;
; known issue. installer induced uninstaller abortion causes overwrite
; by installer without uninstall.
; v3.1.0   - New source version.  Install and uninstall i18n files.
; v3.0.0   - Version for pidgin-2.0.0
; v3.0.0   - Bump version number.
; v2.0.2   - Bump version number.
; v2.0.1   - Bump version number.
; v2.0.0-2 - linking to libotr-2.0.1
; v2.0.0   - Bump version number. Fixed upgrading gaim2-otr (it didn't overwrite the dll)
;            bug reported by Aldert Hazenberg <aldert@xelerance.com>
;          - Added many safeguards and fixed conditions of failures when gaim is running
;             during install, or failed to (un)install previously.
;           - Removed popup signifying gaim is found
; v1.99.0-1 - Bump version number, install Protocol.txt file
; v1.0.3-2  - Fix for detecting gaim if not installed by Administrator
;             bug report by Joanna Rutkowska <joanna@mailsnare.net>
;           - Fix for uninstalling the dll when not installed as Administrator
; v1.0.3    - Initial version


; todo: SetBrandingImage
; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "pidgin-otr"
!define PRODUCT_VERSION "3.1.0-1"
!define PRODUCT_PUBLISHER "Cypherpunks CA"
!define PRODUCT_WEB_SITE "http://otr.cypherpunks.ca/"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "c:\otr\COPYING.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}.exe"
InstallDir "$PROGRAMFILES\pidgin-otr"
InstallDirRegKey HKEY_LOCAL_MACHINE SOFTWARE\pidgin-otr "Install_Dir"
;WriteRegStr HKLM "SOFTWARE\pidgin-otr" "pidgindir" ""

Var "PidginDir"

ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
    ;InstallDir "$PROGRAMFILES\Pidgin\plugins"

    ; uninstall previous pidgin-otr install if found.
    Call UnInstOld
    ;Check for pidgin installation
    Call GetPidginInstPath
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\pidgin-otr" "pidgindir" "$PidginDir"

    SetOutPath "$PidginDir\locale"
    SetOverwrite on
    ; What the next line means is to recursively search c:\otr\locale
    ; and install all files under there named pidgin-otr.mo
    File /r "c:\otr\locale\pidgin-otr.mo"

    SetOutPath "$INSTDIR"
    SetOverwrite on
    File "c:\otr\pidgin-otr.dll"
    ; move to pidgin plugin directory, check if not busy (pidgin is running)
    call CopyDLL
    ; hard part is done, do the rest now.
    SetOverwrite on	  
    File "c:\otr\README.Toolkit.txt"
    File "c:\otr\README.txt"
    File "c:\otr\Protocol-v2.html"
    File "c:\otr\COPYING.txt"
    File "c:\otr\COPYING.LIB.txt"
    File "c:\otr\otr_mackey.exe"
    File "c:\otr\otr_modify.exe"
    File "c:\otr\otr_parse.exe"
    File "c:\otr\otr_readforge.exe"
    File "c:\otr\otr_remac.exe"
    File "c:\otr\otr_sesskeys.exe"
    File "c:\otr\pidgin-otr.nsi"
SectionEnd

Section -AdditionalIcons
  CreateDirectory "$SMPROGRAMS\pidgin-otr"
  CreateShortCut "$SMPROGRAMS\pidgin-otr\Uninstall.lnk" "$INSTDIR\pidgin-otr-uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\pidgin-otr-uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\pidgin-otr-uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
 
SectionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$INSTDIR\pidgin-otr-uninst.exe"
  Delete "$INSTDIR\README.Toolkit.txt"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\Protocol-v2.txt"
  Delete "$INSTDIR\COPYING.txt"
  Delete "$INSTDIR\COPYING.LIB.txt"
  Delete "$INSTDIR\otr_mackey.exe"
  Delete "$INSTDIR\otr_modify.exe"
  Delete "$INSTDIR\otr_parse.exe"
  Delete "$INSTDIR\otr_readforge.exe"
  Delete "$INSTDIR\otr_remac.exe"
  Delete "$INSTDIR\otr_sesskeys.exe"
  Delete "$INSTDIR\pidgin-otr.nsi"
  Delete "$SMPROGRAMS\pidgin-otr\Uninstall.lnk"
  RMDir "$SMPROGRAMS\pidgin-otr"
  RMDir "$INSTDIR"
  
	ReadRegStr $PidginDir HKLM Software\pidgin-otr "pidgindir"
	IfFileExists "$PidginDir\plugins\pidgin-otr.dll" dodelete
  ReadRegStr $PidginDir HKCU Software\pidgin-otr "pidgindir"
	IfFileExists "$PidginDir\plugins\pidgin-otr.dll" dodelete
	
  ReadRegStr $PidginDir HKLM Software\pidgin-otr "pidgindir"
	IfFileExists "$PidginDir\plugins\pidgin-otr.dll" dodelete
  ReadRegStr $PidginDir HKCU Software\Pidgin-otr "pidgindir"
	IfFileExists "$PidginDir\plugins\pidgin-otr.dll" dodelete
  MessageBox MB_OK|MB_ICONINFORMATION "Could not find pidgin plugin directory, pidgin-otr.dll not uninstalled!" IDOK ok
dodelete:
	Delete "$PidginDir\plugins\pidgin-otr.dll"

	; Find all the language dirs and delete pidgin-otr.mo in all of them
	Push $0
	Push $1
	FindFirst $0 $1 $PidginDir\locale\*
	loop:
		StrCmp $1 "" loopdone
		Delete $PidginDir\locale\$1\LC_MESSAGES\pidgin-otr.mo
		FindNext $0 $1
		Goto loop
	loopdone:
	Pop $1
	Pop $0
	
	IfFileExists "$PidginDir\plugins\pidgin-otr.dll" 0 +2
		MessageBox MB_OK|MB_ICONINFORMATION "pidgin-otr.dll is busy. Probably Pidgin is still running. Please delete $PidginDir\plugins\pidgin-otr.dll manually."

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\pidgin-otr\pidgindir"
ok:
SetAutoClose true
SectionEnd

Function GetPidginInstPath
  Push $0
  ReadRegStr $0 HKLM "Software\pidgin" ""
	IfFileExists "$0\pidgin.exe" cont
	ReadRegStr $0 HKCU "Software\pidgin" ""
	IfFileExists "$0\pidgin.exe" cont
  MessageBox MB_OK|MB_ICONINFORMATION "Failed to find Pidgin installation."
		Abort "Failed to find Pidgin installation. Please install Pidgin first."
cont:
	StrCpy $PidginDir $0
	;MessageBox MB_OK|MB_ICONINFORMATION "Pidgin plugin directory found at $PidginDir\plugins ."
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\pidgin-otr" "pidgindir" "$PidginDir"
FunctionEnd

Function UnInstOld
	  Push $0
	  ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"
		IfFileExists "$0" deinst cont
	deinst:
		MessageBox MB_OK|MB_ICONEXCLAMATION  "pidgin-otr was already found on your system and will first be uninstalled"
		; the uninstaller copies itself to temp and execs itself there, so it can delete 
		; everything including its own original file location. To prevent the installer and
		; uninstaller racing you can't simply ExecWait.
		; We hide the uninstall because otherwise it gets really confusing window-wise
		;HideWindow
		  ClearErrors
			ExecWait '"$0" _?=$INSTDIR'
			IfErrors 0 cont
				MessageBox MB_OK|MB_ICONEXCLAMATION  "Uninstall failed or aborted"
				Abort "Uninstalling of the previous version gave an error. Install aborted."
			
		;BringToFront
	cont:
		;MessageBox MB_OK|MB_ICONINFORMATION "No old pidgin-otr found, continuing."
		
FunctionEnd

Function CopyDLL
SetOverwrite try
ClearErrors
; 3 hours wasted so you guys don't need a reboot!
; Rename /REBOOTOK "$INSTDIR\pidgin-otr.dll" "$PidginDir\plugins\pidgin-otr.dll"
IfFileExists "$PidginDir\plugins\pidgin-otr.dll" 0 copy ; remnant or uninstall prev version failed
Delete "$PidginDir\plugins\pidgin-otr.dll"
copy:
ClearErrors
Rename "$INSTDIR\pidgin-otr.dll" "$PidginDir\plugins\pidgin-otr.dll"
IfErrors dllbusy
	Return
dllbusy:
	MessageBox MB_RETRYCANCEL "pidgin-otr.dll is busy. Please close Pidgin (including tray icon) and try again" IDCANCEL cancel
	Delete "$PidginDir\plugins\pidgin-otr.dll"
	Goto copy
	Return
cancel:
	Abort "Installation of pidgin-otr aborted"
FunctionEnd
