@echo off
setlocal

set DRIVER_SERVICE=SunshineDualSenseBus

sc stop %DRIVER_SERVICE% >nul 2>&1
sc delete %DRIVER_SERVICE%
