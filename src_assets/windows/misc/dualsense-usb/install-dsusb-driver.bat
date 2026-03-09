@echo off
setlocal

set DRIVER_SERVICE=SunshineDualSenseBus

if "%~1"=="" (
  echo Usage: %~nx0 ^<path-to-dsusb_bus_driver.sys^>
  exit /b 1
)

set DRIVER_SYS=%~f1

if not exist "%DRIVER_SYS%" (
  echo Driver binary not found: "%DRIVER_SYS%"
  exit /b 1
)

sc qc %DRIVER_SERVICE% >nul 2>&1
if %ERRORLEVEL%==0 (
  sc stop %DRIVER_SERVICE% >nul 2>&1
  sc config %DRIVER_SERVICE% binPath= "\"%DRIVER_SYS%\"" start= demand type= kernel >nul
) else (
  sc create %DRIVER_SERVICE% binPath= "\"%DRIVER_SYS%\"" start= demand type= kernel DisplayName= "Sunshine DualSense USB Bus" >nul
)

sc description %DRIVER_SERVICE% "Sunshine Virtual USB DualSense bus driver."
sc start %DRIVER_SERVICE%
