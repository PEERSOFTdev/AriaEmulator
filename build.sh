#!/bin/sh
# Sestaveni emulatoru krizovym prekladacem mingw-w64 na Linuxu nebo ve WSL.
# Protejsek souboru build.bat. Na Ubuntu jsou to balicky
# g++-mingw-w64-x86-64 a binutils-mingw-w64-x86-64; jiny prefix se zada
# promennou TOOLPREFIX.

set -eu

cd "$(dirname "$0")"

TOOLPREFIX="${TOOLPREFIX:-x86_64-w64-mingw32-}"

for tool in g++ gcc windres; do
  if ! command -v "${TOOLPREFIX}${tool}" >/dev/null 2>&1; then
    echo "Nenašel jsem ${TOOLPREFIX}${tool}."
    echo "Na Ubuntu ho dodají balíčky g++-mingw-w64-x86-64 a binutils-mingw-w64-x86-64."
    echo "Jiný prefix překladače se zadá proměnnou TOOLPREFIX."
    exit 1
  fi
done

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

# CXX, CC a RC jako prirazeni na prikazove radce, ta v make prebiji i `:=`.
make -j"$JOBS" \
  CXX="${TOOLPREFIX}g++" \
  CC="${TOOLPREFIX}gcc" \
  RC="${TOOLPREFIX}windres" \
  "${1:-all}"

echo "Vytvořeno: bin/AriaEmulator.exe"
