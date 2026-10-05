#!/bin/sh
# Sestaveni emulatoru a testu krizovym prekladacem. Protejsek build-tests.bat.

set -eu

cd "$(dirname "$0")"

./build.sh tests
echo "Vytvořeno: bin/codec_test.exe, bin/flash_selftest.exe, bin/firmware_test.exe"
