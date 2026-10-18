#!/bin/sh
# Stahne data instrukcnich testu CPU pro cpu_test (HANDOFF 6.12): sady
# SingleStepTests 8088 a V20 z GitHubu, MIT, do slozky mimo strom. Je to
# presne ta podmnozina, na ktere se ladilo jadro (stazena 5. 10. 2026):
#   - 8088: cela v2_binary (MOO.gz, namereno na AMD D8088) a v2/metadata.json;
#   - V20: z v1_native jen opkody rozsireni 80186 (60h-62h, 68h-6Fh, C0h
#     a C1h po podopkodech, C8h, C9h) a jeji metadata.json;
# u obou README.md a LICENSE. Asi 560 MB.
#
# Volani:  ./get-cputests.sh SLOZKA          (napr. /mnt/d/aria/cputests)
#          ARIA_CPUTESTS=SLOZKA ./get-cputests.sh
#
# Obe repozitare jsou pripichnute na posledni commit ze 5. 10. 2026: sada V20
# se 17. 10. 2026 zmenila a vyjimky v tests/cpu_test.cpp jsou spocitane na
# starsi data. Stahuje git (ridke stazeni jen tech cest, bez API GitHubu
# a jeho limitu); obsah kazdeho souboru tak overi hash gitu, gzip -t pak
# overi, ze se da rozbalit.

set -eu

REPO_8088=aea84484abc79d09639d855b7b0ab32bc9e4dbeb   # 3. 9. 2025
REPO_V20=9efbd02b8ec1a3aad347c2b59672ad25f3bcdb21    # 19. 8. 2025

# Cesty se smeji zadat linuxove i windowsove, jako v run-tests.sh.
to_unix() {
  case "$1" in
    [A-Za-z]:[\\/]*) wslpath -u "$1" ;;
    *) printf '%s' "$1" ;;
  esac
}

TARGET="${1:-${ARIA_CPUTESTS:-}}"
if [ -z "$TARGET" ]; then
  echo "Zadejte cílovou složku argumentem nebo proměnnou ARIA_CPUTESTS,"
  echo "například: ./get-cputests.sh /mnt/d/aria/cputests"
  exit 1
fi
TARGET="$(to_unix "$TARGET")"
if [ -f "$TARGET/8088/metadata.json" ] && [ -f "$TARGET/v20/metadata.json" ]; then
  echo "Data instrukčních testů už ve složce $TARGET jsou, nic se nestahuje."
  exit 0
fi
if [ -e "$TARGET" ] && [ -n "$(ls -A "$TARGET" 2>/dev/null)" ]; then
  echo "Složka $TARGET není prázdná a data testů v ní nejsou celá."
  echo "Zvolte prázdnou nebo neexistující složku."
  exit 1
fi
if ! command -v git >/dev/null 2>&1; then
  echo "Nenašel jsem git, bez něj se data stáhnout nedají."
  exit 1
fi

# Rozpracovane stazeni vedle cile; do cile se presune az cele a overene,
# takze preruseny beh nenecha polovicni data, ktera by run-tests vzal.
WORK="$TARGET.stahovani"
rm -rf "$WORK"
mkdir -p "$WORK"

# fetch REPO COMMIT CESTA... -- ridke stazeni jednoho commitu do $WORK/REPO.
fetch() {
  repo="$1"
  commit="$2"
  shift 2
  echo "Stahuji SingleStepTests/$repo ($commit)..."
  git init -q "$WORK/$repo"
  git -C "$WORK/$repo" remote add origin "https://github.com/SingleStepTests/$repo.git"
  git -C "$WORK/$repo" sparse-checkout set --no-cone "$@"
  git -C "$WORK/$repo" fetch -q --depth 1 --filter=blob:none origin "$commit"
  git -C "$WORK/$repo" checkout -q FETCH_HEAD
}

fetch 8088 "$REPO_8088" /LICENSE /README.md /v2/metadata.json '/v2_binary/*'

V20_PATHS="/LICENSE /README.md /v1_native/metadata.json"
for opcode in 60 61 62 68 69 6A 6B 6C 6D 6E 6F C8 C9; do
  V20_PATHS="$V20_PATHS /v1_native/$opcode.json.gz"
done
for reg in 0 1 2 3 4 5 6 7; do
  V20_PATHS="$V20_PATHS /v1_native/C0.$reg.json.gz /v1_native/C1.$reg.json.gz"
done
# shellcheck disable=SC2086  # cesty jsou bez mezer a maji se rozdelit
fetch v20 "$REPO_V20" $V20_PATHS

# Rozlozeni, ktere ceka cpu_test: SLOZKA/8088/{v2_binary,metadata.json},
# SLOZKA/v20/{v1_native,metadata.json}.
OUT="$WORK/data"
mkdir -p "$OUT/8088" "$OUT/v20"
mv "$WORK/8088/v2_binary" "$WORK/8088/LICENSE" "$WORK/8088/README.md" "$OUT/8088/"
mv "$WORK/8088/v2/metadata.json" "$OUT/8088/"
mv "$WORK/v20/v1_native" "$WORK/v20/LICENSE" "$WORK/v20/README.md" "$OUT/v20/"
mv "$OUT/v20/v1_native/metadata.json" "$OUT/v20/"

count_8088=$(find "$OUT/8088/v2_binary" -name '*.gz' | wc -l)
count_v20=$(find "$OUT/v20/v1_native" -name '*.gz' | wc -l)
if [ "$count_v20" -ne 29 ] || [ "$count_8088" -lt 300 ]; then
  echo "Stažená data nejsou celá: 8088 $count_8088 souborů, V20 $count_v20 z 29."
  exit 1
fi
echo "Ověřuji, že se všechny soubory dají rozbalit..."
if ! find "$OUT" -name '*.gz' -exec gzip -t {} +; then
  echo "Některý soubor je poškozený, data se do $TARGET nepřesunou."
  exit 1
fi

printf 'SingleStepTests 8088 %s\nSingleStepTests v20 %s\nStazeno %s skriptem get-cputests.sh\n' \
  "$REPO_8088" "$REPO_V20" "$(date +%Y-%m-%d)" > "$OUT/ZDROJ.txt"
mkdir -p "$(dirname "$TARGET")"
rmdir "$TARGET" 2>/dev/null || true
mv "$OUT" "$TARGET"
rm -rf "$WORK"
echo "Hotovo: $count_8088 souborů 8088 a $count_v20 souborů V20 v $TARGET."
