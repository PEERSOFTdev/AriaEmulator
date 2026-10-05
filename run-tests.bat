@echo off
setlocal
cd /d "%~dp0"

rem Sestavi testy a pusti je naraz. Ocekava se DESET radku PASS: codec_test,
rem flash_selftest a osm rezimu firmware_test (jedna aktualizace od Robotronu
rem na rezim). Bez firmwaru jen DVA. Protejsek run-tests.sh.
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
set "EXPECTED=10"
if not exist "%ARIAFW%\CARD\" (
  echo Nenasel jsem firmware Arie: %ARIAFW%
  echo Zadejte slozku s podslozkami CARD a SERIAL argumentem nebo promennou ARIAFW.
  echo Testy s firmwarem se vynechaji, PASS budou jen dva a neni to regrese.
  set "SKIP_FIRMWARE=1"
  set "EXPECTED=2"
)

call "%~dp0build-tests.bat"
if errorlevel 1 exit /b 1

if "%JOBS%"=="" set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=4"

echo.
echo === Testy (ocekavano PASS: %EXPECTED%) ===
rem -k dobehne i po prvnim selhani; --output-sync drzi vypis testu pohromade.
rem Make vraci chybu, kdyz kterykoli test selze, takze pocitani PASS jako
rem v run-tests.sh tu neni potreba: vynechane rezimy nejsou cile vubec.
mingw32-make -j%JOBS% -k --output-sync=target MINGW64="%MINGW64:\=/%" FW="%ARIAFW:\=/%" check
if errorlevel 1 (
  echo.
  echo VYSLEDEK: testy NEPROSLY, viz radky vyse.
  exit /b 1
)

echo.
echo VYSLEDEK: vsechny testy prosly.
