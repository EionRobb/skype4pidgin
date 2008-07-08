; Script based on Off-the-Record Messaging NSI file


SetCompress auto
SetCompressor lzma

; todo: SetBrandingImage
; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "skype4pidgin"
!define PRODUCT_VERSION "03-Jul-2008"
!define PRODUCT_PUBLISHER "Eion Robb"
!define PRODUCT_WEB_SITE "http://skype4pidgin.googlecode.com/"
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
!insertmacro MUI_PAGE_LICENSE "COPYING.txt"
; Directory page
;!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_SHOWREADME "http://myjobspace.co.nz/images/pidgin/README.txt"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Run Pidgin"
!define MUI_FINISHPAGE_RUN_FUNCTION "RunPidgin"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
;!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME}-installer.exe"
;InstallDir "$PROGRAMFILES\skype4pidgin"
;InstallDirRegKey HKEY_LOCAL_MACHINE SOFTWARE\skype4pidgin "Install_Dir"
;WriteRegStr HKLM "SOFTWARE\skype4pidgin" "pidgindir" ""

Var "PidginDir"

ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
    ;InstallDir "$PROGRAMFILES\Pidgin\plugins"

    ; uninstall previous install if found.
    ;Call UnInstOld
    ;Check for pidgin installation
    Call GetPidginInstPath
    ;WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\skype4pidgin" "pidgindir" "$PidginDir"
    
    SetOverwrite try
    
	SetOutPath "$PidginDir\pixmaps\pidgin"
	File "/oname=protocols\16\skype.png" "icons\16\skype.png"
	File "/oname=protocols\22\skype.png" "icons\22\skype.png"
	File "/oname=protocols\48\skype.png" "icons\48\skype.png"
	File "/oname=protocols\16\skypeout.png" "icons\16\skypeout.png"
	File "/oname=protocols\22\skypeout.png" "icons\22\skypeout.png"
	File "/oname=protocols\48\skypeout.png" "icons\48\skypeout.png"

	SetOutPath "$PidginDir\pixmaps\pidgin\emotes\skype"
	File "theme"

	SetOutPath "$PidginDir\locale"
	File /nonfatal "/oname=ja\LC_MESSAGES\skype4pidgin.mo" "po\ja.mo"
	File /nonfatal "/oname=de\LC_MESSAGES\skype4pidgin.mo" "po\de.mo"
	File /nonfatal "/oname=fr\LC_MESSAGES\skype4pidgin.mo" "po\fr.mo"
	File /nonfatal "/oname=es\LC_MESSAGES\skype4pidgin.mo" "po\es.mo"
	File /nonfatal "/oname=hu\LC_MESSAGES\skype4pidgin.mo" "po\hu.mo"
	File /nonfatal "/oname=nb\LC_MESSAGES\skype4pidgin.mo" "po\nb.mo"
	File /nonfatal "/oname=it\LC_MESSAGES\skype4pidgin.mo" "po\it.mo"
	File /nonfatal "/oname=ru\LC_MESSAGES\skype4pidgin.mo" "po\ru.mo"
	File /nonfatal "/oname=pl\LC_MESSAGES\skype4pidgin.mo" "po\pl.mo"
	File /nonfatal "/oname=pt\LC_MESSAGES\skype4pidgin.mo" "po\pt.mo"
	File /nonfatal "/oname=en_AU\LC_MESSAGES\skype4pidgin.mo" "po\en_AU.mo"

    SetOverwrite try
	copy:
		ClearErrors
		Delete "$PidginDir\plugins\libskype.dll"
		IfErrors dllbusy
		SetOutPath "$PidginDir\plugins"
	    File "libskype.dll"
		Goto after_copy
	dllbusy:
		MessageBox MB_RETRYCANCEL "libskype.dll is busy. Please close Pidgin (including tray icon) and try again" IDCANCEL cancel
		Goto copy
	cancel:
		Abort "Installation of skype4pidgin aborted"
	after_copy:
		
SectionEnd

Function RegisterURIHandler
  DeleteRegKey HKCR "skype"
  WriteRegStr HKCR "skype" "" "URL:skype"
  WriteRegStr HKCR "skype" "URL Protocol" ""
  WriteRegStr HKCR "skype\DefaultIcon" "" "$PidginDir\pidgin.exe"
  WriteRegStr HKCR "skype\shell" "" ""
  WriteRegStr HKCR "skype\shell\Open" "" ""
  WriteRegStr HKCR "skype\shell\Open\command" "" "$PidginDir\pidgin.exe --protocolhandler=%1"
FunctionEnd

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
FunctionEnd

Function RunPidgin
	ExecShell "" "$PidginDir\pidgin.exe"
FunctionEnd

Function UnInstOld
	  Push $0
	  ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"
		IfFileExists "$0" deinst cont
	deinst:
		  ClearErrors
			;ExecWait '"$0" _?=$INSTDIR'
			ExecWait '"$0" _?="$PROGRAMFILES\skype4pidgin"'
			;IfErrors 0 cont
			;	MessageBox MB_OK|MB_ICONEXCLAMATION  "Uninstall failed or aborted"
			;	Abort "Uninstalling of the previous version gave an error. Install aborted."
			
	cont:
	  DeleteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
		
FunctionEnd
