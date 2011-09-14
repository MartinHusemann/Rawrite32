; Rawrite32 simple installer
;---------------------------
;General

  ;Name and file
  !define PRODUCT_VERSION		"1.0.2.0"
  !define PRODUCT_NAME			"Rawrite32"
  !define PRODUCT_PUBLISHER		"Martin Husemann"
  !define PRODUCT_URL			"http://www.netbsd.org/~martin/rawrite32"
  
  !define PRODUCT_UNINSTALL "${PRODUCT_NAME} uninstall"
  !define INSTALLER_EXE_NAME "rw32-setup-${PRODUCT_VERSION}.exe"
  
  OutFile "${INSTALLER_EXE_NAME}"
  InstallDir "$PROGRAMFILES\Rawrite32"
  InstallDirRegKey HKCU "Software\Rawrite32\${PRODUCT_NAME}" ""

  ;SetCompressor zlib
  ;SetCompressor bzip2
  SetCompressor /SOLID lzma
  SetCompressorDictSize 20
  
  XPStyle On
  
;--------------------------------
;Include Modern UI and Libs

  !include "MUI.nsh"
  !include "Library.nsh"

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING
  !define MUI_HEADERIMAGE

  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY "Software\Rawrite32\${PRODUCT_NAME}" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH
  
;--------------------------------
;Languages
  !verbose push
  !verbose ${MUI_VERBOSE}
 
  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Japanese"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "French"

  !insertmacro MUI_RESERVEFILE_LANGDLL
  
  !verbose pop
  
  Name "${PRODUCT_NAME}"

;--------------------------------
;Definitions

!define UNINSTALL_REGISTRY_PATH "Software\Microsoft\Windows\CurrentVersion\Uninstall"

Section ${PRODUCT_NAME}
  SetOverwrite on

  SetShellVarContext all
  
  SetOutPath "$INSTDIR"
  File "Release\Rawrite32.exe"
  SetOutPath "$INSTDIR\help"
  File "help\rawrite32-en.htm"
  SetOutPath "$INSTDIR\help\images"
  File "www\help\images\icon.png"
  File "www\help\images\Rawrite-before-write.png"
  File "www\help\images\Rawrite-during-write.png"
  File "www\help\images\Rawrite-writing-done.png"
  File "www\help\images\Rawrite-hashoptions.png"
  File "www\help\images\Rawrite-help-menu.png"
  File "www\help\images\Rawrite-settings-menu.png"
  File "www\help\images\Rawrite-skip-options.png"

  ;------------------------------
  ;Registry stuff
  ;Store installation folder
  WriteRegStr HKCU "Software\Rawrite32\${PRODUCT_NAME}" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\Rawrite32.exe" "" "$INSTDIR\Rawrite32.exe" 0 SW_SHOWNORMAL "" "${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_UNINSTALL}.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0 SW_SHOWNORMAL "" "${PRODUCT_NAME}"

  ; Allow user to remove application using Add/Remove Software in Control Panel
  WriteRegStr HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "NoModify" "1"
  WriteRegDWORD HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "NoRepair" "1"

  ; Find out installation size
  !include "FileFunc.nsh"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "EstimatedSize" "$0"
  WriteRegStr HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "DisplayIcon" "$INSTDIR\O2CPlayer.ocx"
  WriteRegStr HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}" "URLInfoAbout" "${PRODUCT_URL}"
  
SectionEnd

;--------------------------------
;Functions

;Function .onInit
  ;!insertmacro MUI_LANGDLL_DISPLAY
  ;MoreInfo::GetOSUserinterfaceLanguage
  ;pop $LANGUAGE  
;FunctionEnd

;Function un.onInit
  ;!insertmacro MUI_UNGETLANGUAGE
  ;MoreInfo::GetOSUserinterfaceLanguage
  ;pop $LANGUAGE  
;FunctionEnd

;--------------------------------
;Descriptions

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ;Refer to "All users" paths
  SetShellVarContext All

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_UNINSTALL}.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  
  DeleteRegKey /ifempty HKCU "Software\Rawrite32\${PRODUCT_NAME}"
  DeleteRegKey HKLM "${UNINSTALL_REGISTRY_PATH}\${PRODUCT_NAME}"
  
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\Rawrite32.exe"
  Delete "$INSTDIR\help\rawrite32-en.htm"
  Delete "$INSTDIR\help\images\icon.png"
  Delete "$INSTDIR\help\images\Rawrite-before-write.png"
  Delete "$INSTDIR\help\images\Rawrite-during-write.png"
  Delete "$INSTDIR\help\images\Rawrite-writing-done.png"
  Delete "$INSTDIR\help\images\Rawrite-hashoptions.png"
  Delete "$INSTDIR\help\images\Rawrite-help-menu.png"
  Delete "$INSTDIR\help\images\Rawrite-settings-menu.png"
  Delete "$INSTDIR\help\images\Rawrite-skip-options.png"
  RMDir "$INSTDIR\help\images"
  RMDir "$INSTDIR\help"
  RMDir "$INSTDIR"
SectionEnd
