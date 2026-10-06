# Nástroje na analýzu firmwaru

Čistý Python 3 bez balíčků navíc. Jedinou výjimkou je `ndisasm` z balíčku
`nasm` (na Ubuntu a ve WSL `sudo apt install nasm`), který potřebují
`ports.py`, `ints.py` a `dis.py`. Nástroje jen **čtou**: nic nezapisují do
repozitáře a `flashdisk.py --extract` cíl uvnitř repozitáře odmítne,
protože rozbalené soubory jsou firmware třetí strany (`ROM-NOTICE.txt`).

Obraz disku B: se vybírá stejně jako v emulátoru: první argument, jinak
proměnná `ARIAFLASH`, jinak `CARD\CC0705.ZIP` ve složce z `ARIAFW`
(výchozí `D:\aria\update`, ve WSL se čte jako `/mnt/d/aria/update`).
Přijme se kartový i sériový ZIP, `FLASHDSK.IMG`, díl `ARIAA4.n` i složka.

## Nástroje

- `ariafw.py` — knihovna: načtení obrazu, FAT12, detekce kódování,
  pořadí startu z CONFIG.SYS a AUTOEXEC.BAT, rozpoznání PKLITE, volání
  ndisasm.
- `flashdisk.py [OBRAZ] [--extract SLOŽKA]` — výpis souborů disku B:,
  jmenovka a kódování; volitelně rozbalení mimo repozitář.
- `ariaa4.py ZIP_NEBO_SLOŽKA` — hlavičky dílů sériové aktualizace,
  kontrola délky a CRC-32 a spojení do 2 MB.
- `ports.py [OBRAZ]` — mapa I/O portů po programech: `in`/`out`
  s pevným portem a přes `DX` po `mov dx`.
- `ints.py [OBRAZ]` — kdo které softwarové přerušení instaluje a kdo ho
  volá, v pořadí startu; vektory použité dřív, než je kdo nainstaluje,
  jsou kandidáti na služby boot ROM (HANDOFF 6.1).
- `strings_cz.py [OBRAZ] CESTA [MIN] [--encoding kamenicky|cp852]` —
  řetězce souboru dekódované kódováním obrazu.
- `dis.py [OBRAZ] CESTA [--range OD[:DO]]` — výpis instrukcí programu.

## Rozbalené programy PKLITE

Programy komprimované PKLITE (AAPI, MAINMENU, AWP a další) rozbalí
`bin/pklite_unpack.exe OBRAZ SLOŽKA` (z C++ testů, potřebuje jádro CPU)
do složky mimo repozitář. Když proměnná `ARIA_UNPACKED` ukazuje na tu
složku, berou `ports.py`, `ints.py` a `dis.py` program odtud místo
komprimovaného; bez ní je přeskočí (`dis.py` je odmítne). Složka musí být
ze stejné verze firmwaru jako obraz.

    bin/pklite_unpack.exe D:/aria/update/CARD/CC0705.ZIP 'D:\aria\unpacked\705'
    export ARIA_UNPACKED=/mnt/d/aria/unpacked/705

## Adresy a omezení

Offsety jsou vůči kódu: u EXE za hlavičkou MZ, u COM od `100h`, u SYS od
nuly. U EXE se skenuje jen kód pod počátečním SS (v modelech paměti
Borlandu a Microsoftu je SS skupina dat), aby se data nečetla jako
instrukce. Lineární průchod dekóduje i data uvnitř kódu, takže každý
nález je nutné ověřit pohledem do `dis.py`; jednotlivá „volání“ bývají
šum.

## Příklady

    export ARIAFW=/mnt/d/aria/update
    python3 tools/ints.py
    python3 tools/ports.py /mnt/d/aria/update/SERIAL/CSR0687.ZIP
    python3 tools/dis.py SYSTEM\\VOICE.SYS --range 1C0:270
    python3 tools/strings_cz.py SYSTEM\\MESSAGES.DAT 10
