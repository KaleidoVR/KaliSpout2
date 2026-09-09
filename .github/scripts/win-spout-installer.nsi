RequestExecutionLevel admin
; KaliSpout2 Windows installer — replaces the original Spout 2 OBS Plugin in-place.

Unicode True

!define APPNAME "KaliSpout2"
!define APPVERSION "DebugVersion"
!define RELEASEDIR "release\Release"
!define APPNAMEANDVERSION "KaliSpout2 ${APPVERSION}"
; Upstream Off-World-Live / community installer identity (same win-spout files).
!define LEGACY_APPNAME "Spout 2 OBS Plugin"
!define UNINSTALLER_NAME "uninstall-kalispout2.exe"
!define LEGACY_UNINSTALLER_NAME "uninstall-spout2-plugin.exe"

Name "${APPNAMEANDVERSION}"
InstallDirRegKey HKLM "Software\${APPNAME}" ""
InstallDir "$COMMONPROGRAMDATA\obs-studio\plugins\win-spout"
OutFile "..\..\release\KaliSpout2_Install_v${APPVERSION}.exe"

SetCompressor Zlib

!include "MUI.nsh"
!include "LogicLib.nsh"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "Setup will install KaliSpout2 for OBS Studio.$\r$\n$\r$\nIf an older Spout 2 OBS Plugin is present, it will be replaced automatically (same plugin folder: win-spout).$\r$\n$\r$\nClose OBS Studio before continuing."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

Var LegacyInstallDir
Var ExistingInstallDir

; Prefer an existing KaliSpout2 or legacy Spout install path so upgrades land on the same folder.
Function .onInit
	StrCpy $LegacyInstallDir ""
	StrCpy $ExistingInstallDir ""

	ReadRegStr $ExistingInstallDir HKLM "Software\${APPNAME}" ""
	${If} $ExistingInstallDir == ""
		ReadRegStr $ExistingInstallDir HKLM "Software\${APPNAME}" "InstallDir"
	${EndIf}

	ReadRegStr $LegacyInstallDir HKLM "Software\${LEGACY_APPNAME}" ""
	${If} $LegacyInstallDir == ""
		ReadRegStr $LegacyInstallDir HKLM "Software\${LEGACY_APPNAME}" "InstallDir"
	${EndIf}

	${If} $ExistingInstallDir != ""
		StrCpy $INSTDIR $ExistingInstallDir
	${ElseIf} $LegacyInstallDir != ""
		StrCpy $INSTDIR $LegacyInstallDir
	${EndIf}
FunctionEnd

; Remove Add/Remove Programs entry + Start Menu leftovers for a prior Spout product.
Function RemoveLegacySpoutRegistration
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${LEGACY_APPNAME}"
	DeleteRegKey HKLM "Software\${LEGACY_APPNAME}"
	Delete "$SMPROGRAMS\${LEGACY_APPNAME}\Uninstall Spout2 OBS Plugin.lnk"
	RMDir "$SMPROGRAMS\${LEGACY_APPNAME}"
FunctionEnd

Section "KaliSpout2" Section1
	SetOverwrite on
	AllowSkipFiles off

	; Clear legacy product registration so Apps & Features only shows KaliSpout2.
	Call RemoveLegacySpoutRegistration

	; Drop the old shared uninstaller name if present (both products used it).
	Delete "$INSTDIR\${LEGACY_UNINSTALLER_NAME}"

	SetOutPath "$INSTDIR\bin\64bit"
	File "..\..\${RELEASEDIR}\win-spout\bin\64bit\win-spout.dll"
	File "..\..\deps\Spout2\BUILD\Binaries\x64\Spout.dll"
	File "..\..\deps\Spout2\BUILD\Binaries\x64\SpoutDX.dll"
	File "..\..\deps\Spout2\BUILD\Binaries\x64\SpoutLibrary.dll"

	SetOutPath "$INSTDIR\data\locale\"
	File "..\..\data\locale\en-US.ini"
	File "..\..\data\locale\zh-CN.ini"
	File "..\..\data\locale\pt-BR.ini"
	File "..\..\data\locale\es-ES.ini"

	CreateDirectory "$SMPROGRAMS\${APPNAME}"
	CreateShortCut "$SMPROGRAMS\${APPNAME}\Uninstall KaliSpout2.lnk" "$INSTDIR\${UNINSTALLER_NAME}"
SectionEnd

Section -FinishSection
	; Default value "" is what InstallDirRegKey reads on the next upgrade.
	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"

	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\${UNINSTALLER_NAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\bin\64bit\win-spout.dll"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "KaleidoVR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "HelpLink" "https://github.com/KaleidoVR/KaliSpout2"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "URLInfoAbout" "https://kalivr.com"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${APPVERSION}"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1

	WriteUninstaller "$INSTDIR\${UNINSTALLER_NAME}"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "Install KaliSpout2 into OBS (replaces Spout 2 OBS Plugin in the win-spout folder)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

UninstallText "This will uninstall KaliSpout2 from your system. OBS Studio scenes that used Spout sources/filters will need another Spout plugin afterward."

Section Uninstall
	SectionIn RO
	AllowSkipFiles off

	; Always scrub both product identities in case of a prior side-by-side install.
	Call un.RemoveLegacySpoutRegistration
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	Delete "$INSTDIR\${UNINSTALLER_NAME}"
	Delete "$INSTDIR\${LEGACY_UNINSTALLER_NAME}"
	Delete "$SMPROGRAMS\${APPNAME}\Uninstall KaliSpout2.lnk"

	Delete "$INSTDIR\bin\64bit\win-spout.dll"
	Delete "$INSTDIR\bin\64bit\Spout.dll"
	Delete "$INSTDIR\bin\64bit\SpoutDX.dll"
	Delete "$INSTDIR\bin\64bit\SpoutLibrary.dll"
	Delete "$INSTDIR\data\locale\en-US.ini"
	Delete "$INSTDIR\data\locale\pt-BR.ini"
	Delete "$INSTDIR\data\locale\zh-CN.ini"
	Delete "$INSTDIR\data\locale\es-ES.ini"

	RMDir "$INSTDIR\bin\64bit"
	RMDir "$INSTDIR\bin"
	RMDir "$INSTDIR\data\locale"
	RMDir "$INSTDIR\data"
	RMDir "$INSTDIR"
	RMDir "$SMPROGRAMS\${APPNAME}"
SectionEnd

Function un.RemoveLegacySpoutRegistration
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${LEGACY_APPNAME}"
	DeleteRegKey HKLM "Software\${LEGACY_APPNAME}"
	Delete "$SMPROGRAMS\${LEGACY_APPNAME}\Uninstall Spout2 OBS Plugin.lnk"
	RMDir "$SMPROGRAMS\${LEGACY_APPNAME}"
FunctionEnd

; eof
