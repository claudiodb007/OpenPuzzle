@echo off
setlocal
cd /d "%~dp0"
:newreport
set "report=%~dp0Diagnostico-GPU-%RANDOM%-%RANDOM%.txt"
if exist "%report%" goto newreport
(
 echo OpenPuzzle - diagnostico de leitura das graficas
 echo Nenhuma definicao de potencia ou ventoinhas e alterada.
 echo.
 "%~dp0openpuzzle.exe" --version
 echo.
 "%~dp0openpuzzle.exe" gpu-control capabilities
 echo.
 if exist "%SystemRoot%\System32\amdadlx64.dll" (echo AMD_ADLX_RUNTIME_PRESENT) else (echo AMD_ADLX_RUNTIME_MISSING)
 echo.
 "%~dp0openpuzzle.exe" gpu-control list
 echo.
 "%~dp0openpuzzle.exe" telemetry
 echo.
 "%~dp0openpuzzle-desktop.exe" desktop-capabilities
) >"%report%" 2>&1
echo Relatorio guardado em "%report%"
start "" "%SystemRoot%\System32\notepad.exe" "%report%"
endlocal
