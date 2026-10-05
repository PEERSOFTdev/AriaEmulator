#!/bin/sh
# Sestavi testy a pusti je naraz. Ocekava se DESET radku PASS: codec_test,
# flash_selftest a osm rezimu firmware_test (jedna aktualizace od Robotronu
# na rezim). Bez firmwaru jen DVA a skript to rekne.
#
# Volani:  ./run-tests.sh                    (firmware z promenne ARIAFW)
#          ./run-tests.sh /mnt/d/aria/update
#
# Bezi jen ve WSL: hotove programy jsou EXE pro Windows a spousti je interop.
# Zadna zalozni cesta k firmwaru tu neni -- tento soubor jde do zverejneneho
# stromu a cesta z jednoho stroje by v nem nemela co delat.

set -eu

cd "$(dirname "$0")"

# Cesty se smeji zadat linuxove (/mnt/d/aria) i windowsove (D:\aria).
# Rozhoduje TVAR cesty: `wslpath -w` na ceste, ktera uz windowsova je, vrati
# tise nesmysl (zkusenost z Eureky, 24. 9. 2026).
to_unix() {
  case "$1" in
    [A-Za-z]:[\\/]*) wslpath -u "$1" ;;
    *) printf '%s' "$1" ;;
  esac
}

if ! command -v wslpath >/dev/null 2>&1; then
  echo "Nenašel jsem wslpath, takže tohle není WSL."
  echo "Testy spouštějí EXE pro Windows a bez interopu se nedají změřit."
  echo "Sestavit se dají i tak: ./build-tests.sh"
  exit 1
fi

if [ $# -ge 1 ] && [ -n "$1" ]; then
  ARIAFW="$1"
fi
ARIAFW="${ARIAFW:-}"
SKIP_FIRMWARE=0
EXPECTED=10
if [ -n "$ARIAFW" ]; then
  ARIAFW="$(to_unix "$ARIAFW")"
fi
if [ -z "$ARIAFW" ] || [ ! -d "$ARIAFW/CARD" ]; then
  echo "Nenašel jsem firmware Arie: ${ARIAFW:-(nezadán)}"
  echo "Zadejte složku s podsložkami CARD a SERIAL argumentem nebo proměnnou ARIAFW."
  echo "Testy s firmwarem se vynechají, PASS budou jen dva a není to regrese."
  SKIP_FIRMWARE=1
  EXPECTED=2
  FW_WIN=""
else
  # EXE dostane windowsovou cestu, s lomitky dopredu (Windows je prijima).
  FW_WIN="$(wslpath -w "$ARIAFW" | tr '\\' '/')"
fi
export SKIP_FIRMWARE

./build-tests.sh

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
TOOLPREFIX="${TOOLPREFIX:-x86_64-w64-mingw32-}"

# -k dobehne i po prvnim selhani; --output-sync drzi vypis testu pohromade.
echo
echo "=== Testy (očekáváno PASS: $EXPECTED) ==="
LOG="build/run-tests.log"
if make -j"$JOBS" -k --output-sync=target \
     CXX="${TOOLPREFIX}g++" \
     CC="${TOOLPREFIX}gcc" \
     RC="${TOOLPREFIX}windres" \
     FW="$FW_WIN" \
     check 2>&1 | tee "$LOG"; then
  :
fi
PASSED="$(grep -c '^PASS' "$LOG" || true)"
echo
if [ "$PASSED" -eq "$EXPECTED" ] && ! grep -q '^FAIL' "$LOG"; then
  echo "VÝSLEDEK: všechny testy prošly, PASS $PASSED z $EXPECTED."
else
  echo "VÝSLEDEK: testy NEPROŠLY, PASS $PASSED z $EXPECTED, viz řádky výše."
  exit 1
fi
