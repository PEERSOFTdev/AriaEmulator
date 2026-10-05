@echo off
setlocal
cd /d "%~dp0"

rem Sestavi testy a pusti je naraz. Ocekava se TRINACT radku PASS: tri bez
rem dat (codec_test, flash_selftest, cpu_selftest), osm rezimu firmware_test
rem s firmwarem (ARIAFW) a dva behy cpu_test s daty instrukcnich testu
rem (ARIA_CPUTESTS, jinak D:\aria\cputests; HANDOFF 6.12). Co chybi, se
rem vynecha. Protejsek run-tests.sh.
rem
rem Volani:  run-tests.bat                    (firmware z %ARIAFW%, jinak D:\aria\update)
rem          run-tests.bat D:\cesta\k\update
rem
rem Firmware je material tretich stran a v repozitari neni (ROM-NOTICE.txt).

if "%MINGW64%"=="" set "MINGW64=C:\msys64\mingw64"
set "PATH=%MINGW64%\bin;%PATH%"

if not "%~1"=="" set "ARIAFW=%~1"
if "%ARIAFW%"=="" set "ARIAFW=D:\aria\update"

rem SKIP_FIRMWARE jde do make PROSTREDIM, stejne jako z run-tests.sh.
set "SKIP_FIRMWARE=0"
set "EXPECTED=13"
if not exist "%ARIAFW%\CARD\" (
  echo Nenasel jsem firmware Arie: %ARIAFW%
  echo Zadejte slozku s podslozkami CARD a SERIAL argumentem nebo promennou ARIAFW.
  echo Osm testu s firmwarem se vynecha a neni to regrese.
  set "SKIP_FIRMWARE=1"
)

if "%ARIA_CPUTESTS%"=="" set "ARIA_CPUTESTS=D:\aria\cputests"
set "SKIP_CPU=0"
if not exist "%ARIA_CPUTESTS%\8088\metadata.json" (
  echo Nenasel jsem data instrukcnich testu CPU: %ARIA_CPUTESTS%
  echo Zadejte slozku s podslozkami 8088 a v20 promennou ARIA_CPUTESTS.
  echo Dva testy jadra CPU se vynechaji a neni to regrese.
  set "SKIP_CPU=1"
)

rem Ocekavany pocet podle toho, co je k dispozici.
if "%SKIP_FIRMWARE%"=="1" set /a EXPECTED-=8
if "%SKIP_CPU%"=="1" set /a EXPECTED-=2

call "%~dp0build-tests.bat"
if errorlevel 1 exit /b 1

if "%JOBS%"=="" set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=4"

echo.
echo === Testy (ocekavano PASS: %EXPECTED%) ===
rem -k dobehne i po prvnim selhani; --output-sync drzi vypis testu pohromade.
rem Make vraci chybu, kdyz kterykoli test selze, takze pocitani PASS jako
rem v run-tests.sh tu neni potreba: vynechane rezimy nejsou cile vubec.
mingw32-make -j%JOBS% -k --output-sync=target MINGW64="%MINGW64:\=/%" FW="%ARIAFW:\=/%" CPUTESTS="%ARIA_CPUTESTS:\=/%" check
if errorlevel 1 (
  echo.
  echo VYSLEDEK: testy NEPROSLY, viz radky vyse.
  exit /b 1
)

echo.
echo VYSLEDEK: vsechny testy prosly.
