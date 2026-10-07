@echo off
setlocal
cd /d "%~dp0"

rem Sestavi testy a pusti je naraz. Ocekava se SEDMNACT radku PASS: ctyri bez
rem dat (codec_test, flash_selftest, cpu_selftest, dsp_selftest), osm rezimu
rem firmware_test
rem a pklite_unpack --check s firmwarem (ARIAFW), dva behy cpu_test s daty
rem instrukcnich testu
rem (ARIA_CPUTESTS, jinak D:\aria\cputests; HANDOFF 6.12) a dva behy
rem boot_test (7.05 a 6.87) s firmwarem a jadrem FreeDOS (ARIA_FREEDOS, jinak D:\aria\freedos). Co
rem chybi, se vynecha. Protejsek run-tests.sh.
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
set "EXPECTED=17"
if not exist "%ARIAFW%\CARD\" (
  echo Nenasel jsem firmware Arie: %ARIAFW%
  echo Zadejte slozku s podslozkami CARD a SERIAL argumentem nebo promennou ARIAFW.
  echo Devet testu s firmwarem se vynecha a neni to regrese.
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

if "%ARIA_FREEDOS%"=="" set "ARIA_FREEDOS=D:\aria\freedos"
set "SKIP_FREEDOS=0"
if not exist "%ARIA_FREEDOS%\KERNEL.SYS" (
  echo Nenasel jsem jadro FreeDOS: %ARIA_FREEDOS%\KERNEL.SYS
  echo Zadejte slozku s KERNEL.SYS ^(ke2046, sestaveni 8086^) promennou ARIA_FREEDOS.
  echo Test startu stroje se vynecha a neni to regrese.
  set "SKIP_FREEDOS=1"
)

rem Ocekavany pocet podle toho, co je k dispozici. Start stroje potrebuje
rem firmware i jadro, proto se odecita jen jednou.
if "%SKIP_FIRMWARE%"=="1" set /a EXPECTED-=11
if not "%SKIP_FIRMWARE%"=="1" if "%SKIP_FREEDOS%"=="1" set /a EXPECTED-=2
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
mingw32-make -j%JOBS% -k --output-sync=target MINGW64="%MINGW64:\=/%" FW="%ARIAFW:\=/%" CPUTESTS="%ARIA_CPUTESTS:\=/%" FREEDOS="%ARIA_FREEDOS:\=/%" check
if errorlevel 1 (
  echo.
  echo VYSLEDEK: testy NEPROSLY, viz radky vyse.
  exit /b 1
)

echo.
echo VYSLEDEK: vsechny testy prosly.
