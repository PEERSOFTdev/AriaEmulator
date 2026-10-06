#!/bin/sh
# Sestavi testy a pusti je naraz. Ocekava se PATNACT radku PASS:
#   - tri bez dat: codec_test, flash_selftest, cpu_selftest;
#   - osm rezimu firmware_test (jedna aktualizace od Robotronu na rezim)
#     a pklite_unpack --check, kdyz je firmware (ARIAFW);
#   - dva behy cpu_test (8088 a rozsireni 80186 ze sady V20), kdyz jsou data
#     instrukcnich testu (ARIA_CPUTESTS, HANDOFF 6.12);
#   - boot_test, kdyz je firmware i jadro FreeDOS (ARIA_FREEDOS, ar-4cu).
# Co chybi, skript vynecha a rekne to; pocet PASS se podle toho snizi.
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
EXPECTED=3
if [ -n "$ARIAFW" ]; then
  ARIAFW="$(to_unix "$ARIAFW")"
fi
if [ -z "$ARIAFW" ] || [ ! -d "$ARIAFW/CARD" ]; then
  echo "Nenašel jsem firmware Arie: ${ARIAFW:-(nezadán)}"
  echo "Zadejte složku s podsložkami CARD a SERIAL argumentem nebo proměnnou ARIAFW."
  echo "Devět testů s firmwarem se vynechá a není to regrese."
  SKIP_FIRMWARE=1
  FW_WIN=""
else
  # EXE dostane windowsovou cestu, s lomitky dopredu (Windows je prijima).
  FW_WIN="$(wslpath -w "$ARIAFW" | tr '\\' '/')"
  EXPECTED=$((EXPECTED + 9))
fi
export SKIP_FIRMWARE

# Data instrukcnich testu CPU, mimo strom (HANDOFF 6.12).
ARIA_CPUTESTS="${ARIA_CPUTESTS:-}"
SKIP_CPU=0
if [ -n "$ARIA_CPUTESTS" ]; then
  ARIA_CPUTESTS="$(to_unix "$ARIA_CPUTESTS")"
fi
if [ -z "$ARIA_CPUTESTS" ] || [ ! -f "$ARIA_CPUTESTS/8088/metadata.json" ]; then
  echo "Nenašel jsem data instrukčních testů CPU: ${ARIA_CPUTESTS:-(nezadána)}"
  echo "Zadejte složku s podsložkami 8088 a v20 proměnnou ARIA_CPUTESTS."
  echo "Dva testy jádra CPU se vynechají a není to regrese."
  SKIP_CPU=1
  CPU_WIN=""
else
  CPU_WIN="$(wslpath -w "$ARIA_CPUTESTS" | tr '\\' '/')"
  EXPECTED=$((EXPECTED + 2))
fi
export SKIP_CPU

# Jadro FreeDOS pro start stroje, mimo strom (HANDOFF 6.10). Bez firmwaru
# nema z ceho startovat, takze se vynecha i tak.
ARIA_FREEDOS="${ARIA_FREEDOS:-}"
SKIP_FREEDOS=0
if [ -n "$ARIA_FREEDOS" ]; then
  ARIA_FREEDOS="$(to_unix "$ARIA_FREEDOS")"
fi
if [ -z "$ARIA_FREEDOS" ] || [ ! -f "$ARIA_FREEDOS/KERNEL.SYS" ]; then
  echo "Nenašel jsem jádro FreeDOS: ${ARIA_FREEDOS:-(nezadáno)}/KERNEL.SYS"
  echo "Zadejte složku s KERNEL.SYS (ke2046, sestavení 8086) proměnnou ARIA_FREEDOS."
  echo "Test startu stroje se vynechá a není to regrese."
  SKIP_FREEDOS=1
  FREEDOS_WIN=""
else
  FREEDOS_WIN="$(wslpath -w "$ARIA_FREEDOS" | tr '\\' '/')"
  if [ "$SKIP_FIRMWARE" -eq 0 ]; then
    EXPECTED=$((EXPECTED + 1))
  fi
fi
export SKIP_FREEDOS

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
     CPUTESTS="$CPU_WIN" \
     FREEDOS="$FREEDOS_WIN" \
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
