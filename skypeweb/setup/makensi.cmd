REM requires:
REM - NSIS 3.0+: http://nsis.sourceforge.net/Download
REM - WinRAR
"%programfiles(x86)%\nsis\makensis.exe" setup.nsi
"%programfiles%\winrar\winrar.exe" a -afzip pidgin-skypeweb.zip pidgin-skypeweb.exe Readme.txt
pause