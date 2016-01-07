; Script based on Off-the-Record Messaging NSI file


SetCompress off

; todo: SetBrandingImage
; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "pidgin-skypeweb"
!define PRODUCT_VERSION "v1.1"
!define PRODUCT_PUBLISHER "Eion Robb"
!define PRODUCT_WEB_SITE "http://eion.robbmob.com/"
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
!insertmacro MUI_PAGE_LICENSE "gpl3.txt"
; Directory page
;!define MUI_PAGE_CUSTOMFUNCTION_PRE dir_pre
;!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_SHOWREADME "http://eion.robbmob.com/README.txt"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Run ${PIDGIN_VARIANT}"
!define MUI_FINISHPAGE_RUN_FUNCTION "RunPidgin"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
;!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${INSTALLER_NAME}.exe"
InstallDir "$PROGRAMFILES\${PIDGIN_VARIANT}"

Var "PidginDir"

ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01

    ;Check for pidgin installation
    Call GetPidginInstPath
    
    SetOverwrite try
	
	SetOutPath "$PidginDir"
	File "${JSON_GLIB_DLL}"
    
	SetOutPath "$PidginDir\pixmaps\pidgin"
	File "/oname=protocols\16\skype.png" "icons\16\skype.png"
	File "/oname=protocols\22\skype.png" "icons\22\skype.png"
	File "/oname=protocols\48\skype.png" "icons\48\skype.png"

	SetOutPath "$PidginDir\pixmaps\pidgin\emotes\skype"
	File "theme"

    SetOverwrite try
	
	copy:
		ClearErrors
		Delete "$PidginDir\plugins\libskypeweb.dll"
		IfErrors dllbusy
		SetOutPath "$PidginDir\plugins"
	        File "libskypeweb.dll"
		Goto after_copy
	dllbusy:
		Delete "$PidginDir\plugins\libskypeweb.dllold"
		Rename  "$PidginDir\plugins\libskypeweb.dll" "$PidginDir\plugins\libskypeweb.dllold"
		MessageBox MB_OK "Old version of plugin detected.  You will need to restart ${PIDGIN_VARIANT} to complete installation"
		Goto copy
	after_copy:
		Call RegisterURIHandler
		
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
  ReadRegStr $0 HKLM "Software\${PIDGIN_VARIANT}" ""
	IfFileExists "$0\pidgin.exe" cont
	ReadRegStr $0 HKCU "Software\${PIDGIN_VARIANT}" ""
	IfFileExists "$0\pidgin.exe" cont
		MessageBox MB_OK|MB_ICONINFORMATION "Failed to find ${PIDGIN_VARIANT} installation."
		Abort "Failed to find ${PIDGIN_VARIANT} installation. Please install ${PIDGIN_VARIANT} first."
  cont:
	StrCpy $PidginDir $0
FunctionEnd

Function RunPidgin
	ExecShell "" "$PidginDir\pidgin.exe"
FunctionEnd

