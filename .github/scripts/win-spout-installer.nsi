RequestExecutionLevel admin
; KaliSpout2 Windows installer — replaces the original Spout 2 OBS Plugin in-place.

Unicode True

!define APPNAME "KaliSpout2"
!define APPVERSION "DebugVersion"
!define RELEASEDIR "release\Release"
!define APPNAMEANDVERSION "KaliSpout2 ${APPVERSION}"
; Minimum OBS this build can load on. BuildInstaller.ps1 rewrites these from buildspec.json.
!define MIN_OBS_MAJOR 33
!define MIN_OBS_MINOR 0
; Upstream Off-World-Live / community installer identity (same win-spout files).
!define LEGACY_APPNAME "Spout 2 OBS Plugin"
!define UNINSTALLER_NAME "uninstall-kalispout2.exe"
!define LEGACY_UNINSTALLER_NAME "uninstall-spout2-plugin.exe"
!define APPICON "..\..\logo\kalispout2.ico"

Name "${APPNAMEANDVERSION}"
InstallDirRegKey HKLM "Software\${APPNAME}" ""
InstallDir "$COMMONPROGRAMDATA\obs-studio\plugins\win-spout"
OutFile "..\..\release\KaliSpout2_Install_v${APPVERSION}.exe"

Icon "${APPICON}"
UninstallIcon "${APPICON}"

SetCompressor Zlib

!include "MUI.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!insertmacro GetParent

!define MUI_ICON "${APPICON}"
!define MUI_UNICON "${APPICON}"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "Setup will install KaliSpout2 for OBS Studio 33 or newer.$\r$\n$\r$\nThe plugin goes in ProgramData\obs-studio\plugins\win-spout, with the DLLs next to the data folder. Older copies in bin\64bit and in Program Files are removed.$\r$\n$\r$\nClose OBS Studio before continuing."

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

	SetRegView 64
	ReadRegStr $0 HKLM "SOFTWARE\OBS Studio" ""
	${If} $0 == ""
		MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL \
			"No OBS Studio installation was found in the registry.$\r$\n$\r$\n\
			KaliSpout2 ${APPVERSION} requires OBS Studio ${MIN_OBS_MAJOR}.${MIN_OBS_MINOR} or newer. \
			If you use a portable OBS, extract the portable zip onto your OBS folder instead of running this installer.$\r$\n$\r$\n\
			Continue anyway?" /SD IDOK IDOK obs_version_done
		Abort
		Return
	${EndIf}

	ClearErrors
	GetDLLVersion "$0\bin\64bit\obs64.exe" $R0 $R1
	${If} ${Errors}
		MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL \
			"Could not read the version of$\r$\n$0\bin\64bit\obs64.exe$\r$\n$\r$\n\
			KaliSpout2 ${APPVERSION} requires OBS Studio ${MIN_OBS_MAJOR}.${MIN_OBS_MINOR} or newer. Continue anyway?" /SD IDOK IDOK obs_version_done
		Abort
		Return
	${EndIf}

	IntOp $R2 $R0 >> 16
	IntOp $R3 $R0 & 0x0000FFFF
	IntOp $R4 $R1 >> 16
	IntOp $R5 $R2 * 1000
	IntOp $R5 $R5 + $R3
	IntOp $R6 ${MIN_OBS_MAJOR} * 1000
	IntOp $R6 $R6 + ${MIN_OBS_MINOR}

	${If} $R5 < $R6
		MessageBox MB_ICONSTOP|MB_OK \
			"OBS Studio $R2.$R3.$R4 was found at$\r$\n$0$\r$\n$\r$\n\
			KaliSpout2 ${APPVERSION} requires OBS Studio ${MIN_OBS_MAJOR}.${MIN_OBS_MINOR} or newer \
			and would show 'The following OBS plugins failed to load: win-spout'.$\r$\n$\r$\n\
			Update OBS Studio, or keep an older KaliSpout2 release that matches your OBS version.$\r$\n\
			https://github.com/KaleidoVR/KaliSpout2/releases" /SD IDOK
		Abort
	${EndIf}
	obs_version_done:
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

	; OBS 33 flags bin\64bit and obs-plugins\64bit as Legacy. OBS 34 will stop loading them.
	Delete "$INSTDIR\bin\64bit\win-spout.dll"
	Delete "$INSTDIR\bin\64bit\win-spout.pdb"
	Delete "$INSTDIR\bin\64bit\Spout.dll"
	Delete "$INSTDIR\bin\64bit\SpoutDX.dll"
	Delete "$INSTDIR\bin\64bit\SpoutLibrary.dll"
	RMDir "$INSTDIR\bin\64bit"
	RMDir "$INSTDIR\bin"

	IfFileExists "$PROGRAMFILES64\obs-studio\obs-plugins\64bit\win-spout.dll" 0 legacy_program_files_done
		Delete "$PROGRAMFILES64\obs-studio\obs-plugins\64bit\Spout.dll"
		Delete "$PROGRAMFILES64\obs-studio\obs-plugins\64bit\SpoutDX.dll"
		Delete "$PROGRAMFILES64\obs-studio\obs-plugins\64bit\SpoutLibrary.dll"
		Delete "$PROGRAMFILES64\obs-studio\obs-plugins\64bit\win-spout.dll"
		Delete "$PROGRAMFILES64\obs-studio\obs-plugins\64bit\win-spout.pdb"
	legacy_program_files_done:
	Delete "$PROGRAMFILES64\obs-studio\obs-plugins\uninstall-spout2-plugin.exe"
	RMDir /r "$PROGRAMFILES64\obs-studio\data\obs-plugins\win-spout"

	; Portable OBS: installing into <OBS>\plugins\win-spout should drop the old obs-plugins copy.
	${GetParent} "$INSTDIR" $1
	${GetParent} "$1" $2
	IfFileExists "$2\obs-plugins\64bit\win-spout.dll" 0 portable_legacy_done
		Delete "$2\obs-plugins\64bit\win-spout.dll"
		Delete "$2\obs-plugins\64bit\win-spout.pdb"
		Delete "$2\obs-plugins\64bit\Spout.dll"
		Delete "$2\obs-plugins\64bit\SpoutDX.dll"
		Delete "$2\obs-plugins\64bit\SpoutLibrary.dll"
		RMDir /r "$2\data\obs-plugins\win-spout"
	portable_legacy_done:

	SetOutPath "$INSTDIR"
	File "..\..\${RELEASEDIR}\win-spout\win-spout.dll"
	File "..\..\deps\Spout2\BUILD\Binaries\x64\Spout.dll"
	File "..\..\deps\Spout2\BUILD\Binaries\x64\SpoutDX.dll"
	File "..\..\deps\Spout2\BUILD\Binaries\x64\SpoutLibrary.dll"

	SetOutPath "$INSTDIR\data\locale\"
	File "..\..\data\locale\en-US.ini"
	File "..\..\data\locale\zh-CN.ini"
	File "..\..\data\locale\pt-BR.ini"
	File "..\..\data\locale\es-ES.ini"

	; Branding icon for Apps & Features / Start Menu
	SetOutPath "$INSTDIR"
	File "/oname=kalispout2.ico" "${APPICON}"

	CreateDirectory "$SMPROGRAMS\${APPNAME}"
	CreateShortCut "$SMPROGRAMS\${APPNAME}\Uninstall KaliSpout2.lnk" "$INSTDIR\${UNINSTALLER_NAME}" "" "$INSTDIR\kalispout2.ico"
SectionEnd

Section -FinishSection
	; Default value "" is what InstallDirRegKey reads on the next upgrade.
	WriteRegStr HKLM "Software\${APPNAME}" "" "$INSTDIR"
	WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"

	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\${UNINSTALLER_NAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\kalispout2.ico"
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
	Delete "$INSTDIR\kalispout2.ico"
	Delete "$SMPROGRAMS\${APPNAME}\Uninstall KaliSpout2.lnk"

	Delete "$INSTDIR\win-spout.dll"
	Delete "$INSTDIR\win-spout.pdb"
	Delete "$INSTDIR\Spout.dll"
	Delete "$INSTDIR\SpoutDX.dll"
	Delete "$INSTDIR\SpoutLibrary.dll"
	Delete "$INSTDIR\bin\64bit\win-spout.dll"
	Delete "$INSTDIR\bin\64bit\win-spout.pdb"
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
