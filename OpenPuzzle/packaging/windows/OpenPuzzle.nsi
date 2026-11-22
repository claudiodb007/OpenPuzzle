Unicode true
!include "x64.nsh"

Name "OpenPuzzle ${VERSION}"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\OpenPuzzle\${VERSION}"
RequestExecutionLevel user
SetCompressor /SOLID lzma

Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

!define UninstallKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenPuzzle-${VERSION}"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "OpenPuzzle requires 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd

Section "OpenPuzzle"
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}/*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\OpenPuzzle ${VERSION}.lnk" "$INSTDIR\openpuzzle-ui.exe"
  WriteRegStr HKCU "${UninstallKey}" "DisplayName" "OpenPuzzle ${VERSION}"
  WriteRegStr HKCU "${UninstallKey}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UninstallKey}" "Publisher" "OpenPuzzle Project"
  WriteRegStr HKCU "${UninstallKey}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UninstallKey}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegDWORD HKCU "${UninstallKey}" "NoModify" 1
  WriteRegDWORD HKCU "${UninstallKey}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  SetOutPath "$TEMP"
  !include "${UNINSTALL_FILES}"
  Delete "$SMPROGRAMS\OpenPuzzle ${VERSION}.lnk"
  Delete "$INSTDIR\Uninstall.exe"
  DeleteRegKey HKCU "${UninstallKey}"
  RMDir "$INSTDIR"
SectionEnd
