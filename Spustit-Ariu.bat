@echo off
rem Spusti emulator z bin\ (vyvojovy strom) nebo vedle teto davky (vydani).
rem Parametry se predavaji dal, napriklad: Spustit-Ariu.bat --flash C:\Aria\SERIAL\CSR0687.ZIP
if exist "%~dp0bin\AriaEmulator.exe" (
  start "" "%~dp0bin\AriaEmulator.exe" %*
) else (
  start "" "%~dp0AriaEmulator.exe" %*
)
