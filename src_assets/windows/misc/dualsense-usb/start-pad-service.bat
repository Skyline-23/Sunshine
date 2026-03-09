@echo off
setlocal

for %%I in ("%~dp0\..\..") do set "ROOT_DIR=%%~fI"
set "PADSVC_BIN=%ROOT_DIR%\tools\sunshinepadsvc.exe"

if not exist "%PADSVC_BIN%" (
  echo sunshinepadsvc.exe not found: "%PADSVC_BIN%"
  exit /b 1
)

start "" "%PADSVC_BIN%"
