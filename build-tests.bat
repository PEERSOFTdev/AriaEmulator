@echo off
setlocal
cd /d "%~dp0"

rem Protejsek build-tests.sh. Cil `tests` obsahuje i emulator.
if "%MINGW64%"=="" set "MINGW64=C:\msys64\mingw64"
set "PATH=%MINGW64%\bin;%PATH%"

if not exist "%MINGW64%\bin\mingw32-make.exe" (
  echo Nenasel jsem mingw32-make v %MINGW64%\bin. Nastavte promennou MINGW64.
  exit /b 1
)

if "%JOBS%"=="" set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=4"

mingw32-make -j%JOBS% MINGW64="%MINGW64:\=/%" tests
if errorlevel 1 exit /b 1

echo Vytvoreno: bin\codec_test.exe, bin\flash_selftest.exe, bin\firmware_test.exe
