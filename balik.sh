#!/bin/sh
# Zabali vydani: archiv ZIP pro uzivatele, tentyz bez cisla verze v nazvu,
# zdrojovy archiv jadra FreeDOS a SHA256SUMS.txt. Pousti ho vydani na GitHubu
# a da se pustit i lokalne ve WSL, aby slo balik zkontrolovat predem.
#
# Volani:  ./balik.sh [EXE [CILOVA_SLOZKA]]
#          vychozi bin/AriaEmulator.exe a build/vydani
#
# Verze se bere z promenne VERZE, jinak z git describe jako v Makefile.
#
# Seznam souboru je vycet, ne "zabal bin/": vedle EXE muze lezet firmware,
# a ten se nesiri (PRAVNI.md, body 3.4 a 3.6). Kontrola na konci to hlida
# i podle jmen a velikosti.
#
# Jadro FreeDOS (GNU GPL v2, PRAVNI.md bod 2.4) se stahuje z vydani projektu
# FDOS/kernel a overuje podle SHA256 -- nikdy ne "nejnovejsi", emulator zna
# jen ke2046 (README). Zdrojovy archiv jde do vydani vedle balicku: GPL v2
# oddil 3 povazuje za dostatecne, kdyz jsou zdroje ke stazeni na temze
# miste jako binarka.

set -eu

cd "$(dirname "$0")"

EXE="${1:-bin/AriaEmulator.exe}"
OUT="${2:-build/vydani}"

FREEDOS_URL=https://github.com/FDOS/kernel/releases/download/ke2046
FREEDOS_BIN=ke2046_86f16.zip
FREEDOS_BIN_SHA=3fc4adb1f693526089df3d46a6512f2c2201d6cffdda40eac4b99d666dcd356a
FREEDOS_SRC=ke2046s.zip
FREEDOS_SRC_SHA=0ea71c9276aba7e1e371f42e8e17d4e9223ac4bedeacccce3ee700fc2866c5e3
KERNEL_SHA=00962622560d83d432ff32b77ca88b13329233415e844af6cac3a1a2f09696d0

for tool in curl unzip zip sha256sum; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Chybí nástroj $tool."
    exit 1
  fi
done
if [ ! -f "$EXE" ]; then
  echo "Nenašel jsem $EXE; nejdřív ./build.sh."
  exit 1
fi

if [ -z "${VERZE:-}" ]; then
  VERZE="$(git describe --tags --match 'v[0-9]*' --always --dirty)"
  case "$VERZE" in
    v*) VERZE="${VERZE#v}" ;;
    *) VERZE="0.0.0-$VERZE" ;;
  esac
fi
NAME="AriaEmulator-$VERZE"

# Stazene archivy FreeDOS si drzi build/freedos, aby se pri opakovanem
# baleni nestahovaly znovu; overuji se pokazde.
CACHE=build/freedos
mkdir -p "$CACHE"
fetch() {
  file="$1"
  sha="$2"
  if [ ! -f "$CACHE/$file" ] || ! echo "$sha  $CACHE/$file" | sha256sum -c --status; then
    echo "Stahuji $file z FDOS/kernel..."
    curl -fsSL -o "$CACHE/$file.part" "$FREEDOS_URL/$file"
    mv "$CACHE/$file.part" "$CACHE/$file"
  fi
  if ! echo "$sha  $CACHE/$file" | sha256sum -c --status; then
    echo "$file nemá očekávaný kontrolní součet; do balíku nejde."
    exit 1
  fi
}
fetch "$FREEDOS_BIN" "$FREEDOS_BIN_SHA"
fetch "$FREEDOS_SRC" "$FREEDOS_SRC_SHA"

rm -rf "$OUT"
STAGE="$OUT/stage/$NAME"
mkdir -p "$STAGE/FreeDOS"

cp "$EXE" "$STAGE/AriaEmulator.exe"
cp Spustit-Ariu.bat README.md LICENSE.txt PRAVNI.md "$STAGE/"
cp src/i86/LICENSE.mame-i86.txt src/adsp2100/LICENSE.mame-adsp2100.txt "$STAGE/"

# KERNEL.SYS vedle EXE, kde ho emulator hleda; licence a puvod do FreeDOS/.
unzip -p "$CACHE/$FREEDOS_BIN" bin/kernel.sys > "$STAGE/KERNEL.SYS"
if ! echo "$KERNEL_SHA  $STAGE/KERNEL.SYS" | sha256sum -c --status; then
  echo "KERNEL.SYS z $FREEDOS_BIN nemá očekávaný kontrolní součet."
  exit 1
fi
unzip -p "$CACHE/$FREEDOS_SRC" SOURCE/ke2046/COPYING > "$STAGE/FreeDOS/COPYING.txt"
cat > "$STAGE/FreeDOS/ZDROJ.txt" <<EOF
Jádro FreeDOS (KERNEL.SYS)

KERNEL.SYS vedle AriaEmulator.exe je jádro FreeDOS, verze ke2046,
sestavení pro 8086 s FAT16. Emulátor ho zavádí místo ztraceného jádra
ROM-DOS Arie. Soubor je nezměněný, tak jak ho vydal projekt FDOS/kernel.

Licence: GNU General Public License verze 2, viz COPYING.txt v této
složce. Licence emulátoru (LICENSE.txt) se na jádro nevztahuje.

Původ:
  $FREEDOS_URL/$FREEDOS_BIN
  SHA256 $FREEDOS_BIN_SHA
  KERNEL.SYS SHA256 $KERNEL_SHA

Zdrojové texty přesně této verze:
  $FREEDOS_SRC, přiložený ke stejnému vydání emulátoru jako tento balík,
  a také $FREEDOS_URL/$FREEDOS_SRC
  SHA256 $FREEDOS_SRC_SHA
EOF

# Pojistka proti firmwaru v baliku: obraz flash a dily sériové aktualizace
# podle jmena, archivy ZIP (aktualizace Arie jsou ZIP) a ulozena pamet
# emulatoru, k tomu velikosti obrazu disku B: (1 262 080 B u 7.05, 2 MB
# spojenych dilu).
bad="$(find "$STAGE" -type f \( -iname 'FLASHDSK*' -o -iname 'ARIAA4*' -o -iname '*.zip' \
  -o -iname '*.img' -o -iname '*.dsp' -o -iname 'pamet-*' -o -size 1262080c -o -size 2097152c \) -print)"
if [ -n "$bad" ]; then
  echo "V balíku je něco, co vypadá jako firmware:"
  echo "$bad"
  exit 1
fi

(cd "$OUT/stage" && zip -q -r -X "../$NAME.zip" "$NAME")
cp "$OUT/$NAME.zip" "$OUT/AriaEmulator.zip"
cp "$CACHE/$FREEDOS_SRC" "$OUT/"
rm -rf "$OUT/stage"
(cd "$OUT" && sha256sum "$NAME.zip" AriaEmulator.zip "$FREEDOS_SRC" > SHA256SUMS.txt)

echo "Balík $NAME je v $OUT:"
ls -l "$OUT"
