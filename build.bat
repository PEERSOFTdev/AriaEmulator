@echo off
setlocal
cd /d "%~dp0"

rem mingw64 z msys2. Vlastni prefix jde na zacatek PATH, at se nevezme jiny
rem prekladac z globalni PATH (zkusenost z Eureky). Protejsek build.sh.
if "%MINGW64%"=="" set "MINGW64=C:\msys64\mingw64"
set "PATH=%MINGW64%\bin;%PATH%"

if not exist "%MINGW64%\bin\g++.exe" (
  echo Nenasel jsem g++ v %MINGW64%\bin. Nastavte promennou MINGW64.
  exit /b 1
)
if not exist "%MINGW64%\bin\mingw32-make.exe" (
  echo Nenasel jsem mingw32-make v %MINGW64%\bin. Nastavte promennou MINGW64.
  exit /b 1
)

if "%JOBS%"=="" set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=4"

rem Cesta jde do make s lomitky dopredu: make si podle PATH muze vybrat sh
rem a ten by zpetna lomitka bral jako escape sekvence.
mingw32-make -j%JOBS% MINGW64="%MINGW64:\=/%" all
if errorlevel 1 exit /b 1

echo Vytvoreno: bin\AriaEmulator.exe
