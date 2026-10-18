# Pokyny pro agenty

## Důležité: K EDITOVÁNÍ A VYTVÁŘENÍ SOUBORŮ NEPOUŽÍVEJ PYTHON ANI JINÉ NESTANDARDNÍ NÁSTROJE

Na soubory slouží nástroje Write a Edit. Python, heredoc a `sed` na
víceřádkové úpravy v sourozeneckém projektu Eureka opakovaně rozbíjely
obsah. Python patří jen do `tools/`, a to jako **čtecí** analýza
firmwaru, nikdy jako editor stromu.

## Způsob práce

Kvůli lepšímu hospodaření s limitem postupuj inkrementálně. Nejdřív udělej
část práce, nějaký smysluplný krok, shrň, co jsi udělal, a zastav se. Na
pokyn „pokračuj“ nebo „next“ udělej další krok.

## Nejdřív si přečti HANDOFF.md

`HANDOFF.md` je nosič kontextu: co je zjištěno, čím je to doloženo, co je
otevřené a v jakém pořadí pokračovat. Mapa hardwaru je v
`hardware-map.md`. Otevřené otázky mají trvalá čísla `6.x` — odkazují se na
ně zdrojáky a commity, proto se **nepřečíslovávají**.

`HANDOFF-archiv.md` (až vznikne) nese uzavřené zásahy i s měřeními. Při
startu ho nečti; otevři ho, až když pracuješ na tématu, na které odkazuje
odstavec v `HANDOFF.md`. Zásah se do něj stěhuje, jen když jeho závěr
**opravdu stojí jinde** (v kódu, v mapě, v README).

## Beads evidují otevřenou práci, HANDOFF nese doložení

Projekt používá **beads** (`bd`, prefix `ar`). Dělba je jednoslovná:
**bead je ukazatel na otevřenou práci, HANDOFF je doložení.** Bead nese
název, prioritu, závislosti a odkaz na sekci; proč je něco pravda a čím je
to změřeno, stojí v `HANDOFF.md`.

Hook `bd prime` vkládá pravidla, která si s tímto projektem ve dvou bodech
odporují. Platí toto, ne ona:

- „Do NOT use markdown files for task tracking“ se na `HANDOFF.md`
  **nevztahuje** — není to seznam úkolů, je to nosič doložení.
- „Do NOT use MEMORY.md files“ **neplatí**. `bd remember` nepoužívej:
  automatický export ho nevyváží a obsah by se tiše ztratil.

Před commitem `bd export -o .beads/issues.jsonl`.

## Hlavní omezení projektu: boot ROM chybí

Firmware, který máme, je **jen disk B:** — interní flash s FAT12
(Datalight „DLRDISK“). Boot ROM (BIOS, jádro ROM-DOS, ovladač disku B:
`FLASHDSK.SYS` a případné další služby) je samostatný čip a **není
k dispozici, fyzická Aria taky ne**.
Emulátor ji proto nahrazuje vlastní vysokoúrovňovou emulací (HLE), zatímco
všechno z disku B: — ovladače, AAPI, řečový engine PeterTalk (`SPEAK.EXE`
a `LPC.DSP` pro ADSP-2105) i aplikace — běží jako původní binárky.

Z toho plyne pravidlo: **co boot ROM poskytovala, se odvozuje z toho, kdo
to na disku B: volá**, ne z představy, jak by to asi mohlo být. Každá HLE
služba musí v komentáři citovat binárku a offset, kde ji někdo používá.

A **„nikdo to neinstaluje“ se tvrdí až po `tools/ints.py`**, ne po hledání
jednoho bajtového vzoru. Hledání jen `mov ax,2569h` jednou přisoudilo
`INT 69h` boot ROM, a přitom ho instaluje `VOICE.SYS` přes `mov ah`/`mov al`
(HANDOFF 6.1, oprava z 5. 10. 2026). `ints.py` se pouští s rozbalenými
programy PKLITE (`ARIA_UNPACKED`), jinak je jeho výsledek jen dolní
odhad; i pak každý nález ověř v `dis.py`, jednotlivá „volání“ bývají data.

## Dokumentace Arie — podívej se do ní dřív, než začneš odvozovat

Máme **Aria API Reference** (`DOC/API.DOC`, 69 stran), specifikaci řídicích
sekvencí řeči PeterTalk (`DOC/SPEECH.DOC`), hlavičky `ARIA-INT.H`,
`ARIA-API.H`, ukázkový program a PC verzi `AAPI.EXE`. Je to materiál
třetích stran a **v repozitáři není**: cesta je v proměnné `ARIATECH`,
jinak `D:\aria\techman` (z WSL `/mnt/d/aria/techman`). **Nekopíruj z ní
soubory do repozitáře.** Zjištění z ní se zapisují vlastními slovy do
`hardware-map.md` a `HANDOFF.md`.

Manuál popisuje API (přerušení 60h, 61h, 62h…), ne hardware. Porty se
odvozují z ovladačů na disku B:.

## O uživateli

Uživatel je **nevidomý**, pracuje se čtečkou obrazovky NVDA. Z toho plyne:

- **V konverzaci odpovídej anglicky** (rozhodnutí uživatele z 5. 10. 2026);
  repozitář je česky.
- Formátuj pro čtečku obrazovky: nadpisy a seznamy ano, ASCII grafika a
  zarovnávání mezerami ne. Tabulky střídmě.
- Neodkazuj na to, „co je vidět na obrazovce“. Důležitý výstup nástrojů
  zopakuj v textu.
- Zvuk je celé uživatelské rozhraní emulovaného stroje, ne doplněk. Změny,
  které zhorší řeč, jsou vážné.

## Firmware

Leží **mimo repozitář**, cesta je v proměnné `ARIAFW`, jinak
`D:\aria\update` (z WSL `/mnt/d/aria/update`). Je to firmware třetí strany
— nikdy ho necommituj a nikam neposílej (`PRAVNI.md`). Totéž platí pro
všechno z něj odvozené: rozbalené programy, výpisy paměti, uloženou paměť
emulátoru, proud bajtů pro DSP (`PRAVNI.md`, bod 3.6). Dokumentace
neuvádí, kde firmware získat, a na žádný zdroj neodkazuje (bod 4.2) —
uživatelé Arie to vědí. Hlavní cíl:

- `CARD/CC0705.ZIP` → `FLASHDSK.IMG`, sestavení 7.05, česky, Latin 2.
  ZIP MD5 `0d220f7c41d5b177b2a65c0231c83bb3`, obraz MD5
  `db39e8810023dcc812d9d1775ae6dcf5`, 1 262 080 B.
- Křížová kontrola: `CARD/ECRD0704.ZIP` (anglicky, 7.04), obraz MD5
  `a684464fd48deb54ec2ef09238767eb1`.
- `SERIAL/*/ARIAA4.1–3` jsou tentýž disk ve třech dílech po 256B hlavičce
  „ARIA“; spojené dají přesně 2 MB, tedy rozvržení celé flash.

Uživatel chce běžet i na jiné verzi než 7.05 (hlavně na 6.87 kvůli
Kamenických). Disk B: je proto **volitelný** (`--flash`, `ARIAFLASH`) a nic
v emulátoru nesmí předpokládat konkrétní verzi ani kódování — vždy se bere
z `FlashImage` (`src/flash_image.h`).

## Sestavení a testy

Z WSL krosovým mingw-w64: `./build.sh`, `./build-tests.sh`,
`./run-tests.sh [složka firmwaru]` (jinak z proměnné `ARIAFW`). Výstup
EXE do `bin/`, mezivýstupy do `build/`. Očekává se **osmnáct** řádků
`PASS`: pět bez dat, devět s firmwarem (`ARIAFW`; osm režimů
`firmware_test` a `pklite_unpack --check`), dva s daty instrukčních
testů CPU (`ARIA_CPUTESTS`, výchozí `D:\aria\cputests`, 562 MB,
SingleStepTests; stáhne je `./get-cputests.sh SLOŽKA`) a dva běhy `boot_test` (7.05 a 6.87 v Kamenických),
když je firmware i jádro FreeDOS
(`ARIA_FREEDOS`, výchozí `D:\aria\freedos`, soubor `KERNEL.SYS` ze
sestavení ke2046 pro 8086; GPL, do repozitáře nepatří). Co chybí,
skript vynechá, řekne to a počet sníží.

Balík vydání dělá `./balik.sh` do `build/vydani`; jádro FreeDOS do něj
stahuje sám a ověřuje podle SHA256 (`PRAVNI.md` bod 2.4). Seznam souborů
v balíku je výčet a firmware hlídá kontrola na konci skriptu. Vydání
spouští uživatel skriptem `./release` (workflow `.github/workflows/
vydani.yml`, sestavení `sestaveni.yml` na Windows s msys2); ty ho
nespouštěj. `run-tests.sh` po úplném běhu zapíše `build/otestovany-strom`,
podle kterého se `./release` ptá.

Start stroje se ladí `bin/boot_test.exe ZIP KERNEL.SYS -v [cykly]`
(výstup `INT 10h`, emulované sekundy jednotlivých fází startu, registry,
neznámé porty, stav a log DSP, vzorky kolem přepnutí programu DSP); `-dsp
CESTA` uloží proud bajtů poslaných DSP (mimo strom), `-wav CESTA
FREKVENCE` zvuk ze SPORT1 jako WAV (frekvence pro SCLKDIV 4, HANDOFF
6.3); `-t` krokuje a zastaví na
prvním skoku do prázdné paměti s posledními 40 instrukcemi; `-dump
CESTA` uloží adresní prostor (s `-t` v místě zastavení, jinak na konci
běhu) — **mimo strom**, obsahuje firmware.

Programy PKLITE (AAPI, MAINMENU, AWP…) se pro analýzu rozbalí
`bin/pklite_unpack.exe OBRAZ SLOŽKA` do složky **mimo strom** (např.
`D:\aria\unpacked\705`); nástroje v `tools/` je pak čtou místo
komprimovaných, když je ta složka v proměnné `ARIA_UNPACKED`. Složka musí
odpovídat verzi obrazu.

Z Windows s msys2 (`C:\msys64\mingw64`, jinak proměnná `MINGW64`):
`build.bat`, `build-tests.bat`, `run-tests.bat [složka firmwaru]` (jinak
`ARIAFW`, výchozí `D:\aria\update`). Na stroji, kde vznikly (5. 10. 2026),
msys2 není: přes cmd.exe jsou ověřené jen jejich chybové větve, ne
skutečný překlad.

Analytické nástroje jsou v `tools/` (viz `tools/README.md`). Na tvrzení
o tom, kdo instaluje které přerušení, se používá `tools/ints.py`.

## Adresy

Adresy v souborech z disku B: se uvádějí jako `SOUBOR.EXT+offset`
(šestnáctkově). U EXE komprimovaných PKLITE vždy v **rozbaleném** obrazu
a s poznámkou, že jde o offset za hlavičkou. Adresy v paměti emulovaného
stroje jako `segment:offset`, fyzické s příponou `h` na pět číslic.

## Styl kódu

Emulátor je C++20, dvoumezerové odsazení, metody `PascalCase`, členy
s podtržítkem na konci. **Komentáře ve zdrojácích anglicky**, dokumentace
a uživatelské řetězce česky.

Komentuj *proč*, ne *co*. U hardwarových předpokladů připiš místo
v binárce, které je dokládá — jinak je nikdo později neověří.

### Jádro CPU z MAME (`src/i86/`)

Je to kód MAME pod BSD-3 (`src/i86/LICENSE.mame-i86.txt`), ne pod MIT.
Pravidla (HANDOFF 6.12):

- Kód MAME zůstává **doslovný**, aby šel porovnat s originálem (commit
  `2aa75c50`); styl projektu se na něj nevztahuje (tabulátory, jména
  MAME). Každá naše změna nese komentář `ARIA:` s důvodem a odkazem na
  test, který ji vynutil.
- Opravy chování se dělají **jen podle měření** (`cpu_test`) nebo podle
  dokumentace Intelu pro 80186, nikdy podle odhadu. Kde 8088 a 80186
  mohou chování mít jinak, nesmí se test 8088 „opravit“ úpravou jádra;
  patří do výjimek v `tests/cpu_test.cpp`, s důvodem a počtem ve výpisu.
- Varování GCC v kódu MAME se vypíná v Makefile jen pro `src/i86/`
  (`-Wno-sign-compare`), ne úpravou kódu.
- Ladění jednoho opkódu: `bin/cpu_test.exe D:/aria/cputests 8088 7C -v`
  vypíše vstupní stav a všechny rozdíly prvních selhání.

### Jádro DSP z MAME (`src/adsp2100/`)

Totéž pro ADSP-2105 (`src/adsp2100/LICENSE.mame-adsp2100.txt`, HANDOFF
6.3), commit MAME `2aa75c50`. `2100ops.hxx` a `adsp2100.cpp` jsou kopie
souborů MAME; co patří jen frameworku MAME, je vypnuté bloky `#if 0 //
ARIA:`, ne smazané, aby soubory šly porovnat s MAME řádek po řádku i při
přechodu na novější verzi. Paměti čipu, řídicí registry a zavádění nejsou
součást jádra (ani v MAME), jsou v `src/adsp2105.*`. Instrukční testy
jako u 80188 pro tento DSP neexistují; ověřuje se podle manuálu
(*ADSP-2100 Family User's Manual*, HANDOFF 6.3) a výsledkem. Program DSP
se čte `bin/dsp_disasm.exe SOUBOR.DSP [OD [DO]]` (disasembler MAME,
`2100dasm.cpp` stejně doslovný); výpis je firmware, patří mimo strom.

## Režim, který není slyšet, je chyba

Klávesnice, která nikam nevede, a rozbitá klávesnice jsou **obě potichu**.
Každá změna toho, kam jdou klávesy, proto zní: `Beep()` hostitele, který
nejde zaměnit s hlasem Arie, a stav stojí i v titulku okna (NVDA+T).

## Co znamená hotovo

Dokud neplatí tohle, nehlas hotovo — a nehlas ani „mělo by to fungovat“:

1. `build.sh` / `build.bat` projde bez jediného varování.
2. `run-tests` dá očekávaný počet řádků `PASS` (počet je uveden v
   `run-tests.sh`).
3. Dokumentace doběhla **ve stejném kroku**: README, když se změnilo
   chování; HANDOFF, když v něm něco přestalo platit (odstavec se nemaže,
   připíše se pod něj, co už neplatí a proč).
4. Uživatelské řetězce se ověřují na hotovém EXE v UTF-16, ne na zdrojáku.

Až potom se ptej na commit. Když něco nevyšlo, řekni to rovnou i s
výstupem.

## Git

Commituj jen, když o to uživatel požádá. Nepushuj bez vyzvání. Zprávy
commitů česky, s odkazem na sekci HANDOFF (`HANDOFF 6.3`) nebo bead
(`ar-xxx`).

Konce řádků drží `.gitattributes`, ne ty: `*.bat` CRLF, `*.sh`, `*.py` a
`/release` LF.
