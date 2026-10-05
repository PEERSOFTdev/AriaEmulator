# Aria — stav práce

Reverzní inženýrství a emulátor kapesního počítače pro nevidomé **Aria**
(Robotron, nástupce Eureky A4, kolem roku 1996).

Tento dokument je nosič kontextu: co je zjištěno, čím je to doloženo a co
je otevřené. Mapa hardwaru je v `hardware-map.md`. Otevřená práce se eviduje
v beads (`bd`, prefix `ar`); tady stojí doložení. Čísla `6.x` se
nepřečíslovávají.

---

## 1. Zdroj

### Firmware (`ARIAFW`, jinak `D:\aria\update`)

- `CARD/` — kartové verze. Každý ZIP obsahuje jediný `FLASHDSK.IMG`:
  - `ariav699.zip` — sestavení 6.99 (1998-11-24), 1 240 576 B
  - `CC0705.ZIP` — **7.05 česky (Latin 2)**, 1 262 080 B — hlavní cíl
  - `ECRD0704.ZIP` — 7.04 anglicky, 2 029 056 B — křížová kontrola
  - `TC0690.ZIP` — 6.90 thajsky, 1 687 552 B
- `SERIAL/` — sériové verze, každá po třech dílech `ARIAA4.1–3`:
  `CSR0687` (česky, kód Kamenických), `CSR0705` (česky, Latin 2),
  `ESER0704`, `TSR0690`, a `aserv687.rar` (zatím nerozbaleno).

Popisy verzí pocházejí ze souborů `descript.ion` v týchž adresářích.

### Kódování češtiny podle verze (ověřeno 5. 10. 2026)

Ověřeno dekódováním `SYSTEM\MESSAGES.DAT` („prázdný řádek“, „paměť“,
„tečka“), ne jen podle `descript.ion`:

- **6.87** (`SERIAL/CSR0687`) — **Kamenických**, stejně jako Eureka A4.
  Jediná dochovaná verze v tomto kódování.
- **6.99** (`CARD/ariav699`) — **CP852 (Latin 2)**, i když je starší.
- **7.05** (`CARD/CC0705`, `SERIAL/CSR0705`) — CP852.

Uživatel chce mít možnost běžet na kterékoli z nich (Kamenických je
mezi českými uživateli DOSu nejrozšířenější a kompatibilní s Eurekou).
Obraz disku B: je proto **volitelný** a kódování se pozná z obrazu
(lze ho i vnutit) — `src/flash_image.*`.

### Formát obrazu flash

`FLASHDSK.IMG` je obraz FAT12 s bootovacím záznamem Datalight `DLRDISK`:
512 B/sektor, 1 sektor/cluster, 1 rezervovaný, **1 FAT**, **16 položek
kořene**, médium `F8h`. Bootovací kód je prázdný (`FFh`), jmenovka svazku má 11 bajtů
`Build  705` + `FBh` (fajfka v CP437 a v Kamenických) — číslo verze bez
tečky. Nástroje jako 7z obraz neotevřou kvůli neobvyklému BPB.

Díl `ARIAA4.n` má 256B hlavičku, vše little-endian: `"ARIA"` na offsetu
0, číslo dílu (16 bit) na 4, délka dat (32 bit, např. `000C5000h`) na 6,
**CRC-32 dat** (standardní, polynom `EDB88320h`, jako ZIP) na 10, zbytek
nuly. CRC ověřeno 5. 10. 2026 na všech devíti dílech verzí 6.87, 7.04
a 7.05. Data tří dílů (806 912 + 806 912 + 483 328 B)
dají spojená **přesně 2 097 152 B** — celou 2MB flash. Začíná týmž
`DLRDISK` a obsahuje týž strom souborů jako kartový obraz téže verze;
zbytek je `FFh`. Konec (oblast resetovacího vektoru) je prázdný.

### Dokumentace (`ARIATECH`, jinak `D:\aria\techman`)

- `DOC/API.DOC` — *Aria API Reference* (Word 97, 69 stran, 1994–1999).
- `DOC/SPEECH.DOC` — *PeterTalk specifications*, řídicí sekvence řeči
  `!<..>`; jazyky mj. **Czech-1**, stroje Aria, Esprit, RSC, Sunrise.
- `ARIA-INT.H` — čísla přerušení a funkcí: `INT 60h` AAPI, `INT 61h`
  řeč, `INT 62h` čtení řeči, dále MODE API.
- `ARIA.C`, `ARIA.PAS`, demo (Borland C++), `MAKEMSG.EXE`.
- `AAPI.EXE` (85 374 B) — „verze Aria API, která běží na PC“. Liší se od
  `B:\SYSTEM\AAPI.EXE` (48 135 B, PKLITE).

## 2. Přehled stroje

Podle robotron.net.au/aria.htm: CPU **80C188**, DSP **ADSP-2105**, 1 MB
nevolatilní RAM, 2 MB flash, slot PCMCIA typ I, paralelní a sériový port,
sluchátka, 6 braillských kláves, mezerník, 8 funkčních a 5 kurzorových
kláves, bez displeje, NiCd akumulátor. Operační systém ROM-DOS 7.

### Disky (README.DOC na disku B:)

- `B:` interní flash, `C:` karta PCMCIA, `D:` RAM disk, `E:` a dál
  externí mechaniky Backpack na paralelním portu (od firmwaru 702).

### Start (CONFIG.SYS a AUTOEXEC.BAT z 7.05)

CONFIG.SYS: `shell=b:\command.com b:\ /p /e:512`, ovladače v pořadí
`VOICE.SYS`, `DLFFSA.EXE`, `RAMDISK.EXE`, `BACKPACK.SYS nonstop`,
`BPHDARIA.SYS nonstop`.

AUTOEXEC.BAT: `TRICKSS2`, `aapi -c` (při chybě konec), `speak -i
%SYSDIR%\`, `bkb`, `fossil`, `ariamode`, `arialarm`, `rundos`,
`atkbd.com`, `mainmenu`.

## 3. Řeč

Řečový engine je **PeterTalk** (Peter Veprek, 1992–96), `SPEAK.EXE` —
Borland C++, rezidentní na `INT 61h`, „Speech engine : Cze2/3.0.0“. Za
běhu otevírá `lpc.dsp` a `play.dsp` (programy pro DSP; chybová hláška
„boot DSP LPC buffer too small“) a fonémová data `PT.DAT` („Aria
Cze1/1.3.0“). Hlášky aplikací jsou v `MESSAGES.DAT` (česky).

`SAY.COM` (30 B) posílá každý znak příkazové řádky přes `INT 61h`
`AX=0200h` — nejmenší možný klient řeči, vhodný na první test.

## 4. Klávesnice

Zatím jen z popisu: 6 braillských kláves, mezerník, F1–F8, 5 kurzorových.
Ovladač `BKB.COM` čte port `0700h` (viz 6.4).

## 5. Stav emulátoru

Repozitář založen 5. 10. 2026. Hotové je jen načtení disku B:
(`src/flash_image.*`, `src/fat12.*`, `src/zip_reader.*`,
`src/text_codec.*`): přijme kartový i sériový ZIP, `FLASHDSK.IMG`, díly
`ARIAA4.n` i složku, ověří CRC-32 členů ZIPu i dílů, u sériových dílů
navíc, že dávají celé 2 MB (hlavičky počet dílů neuvádějí, chybějící
poslední díl by se jinak spojil bez chyby), a pozná kódování.
`AriaEmulator.exe` výsledek jen ohlásí oknem se zprávou. Testy:
`codec_test`, `flash_selftest` (bez firmwaru) a `firmware_test` v osmi
režimech, jeden na každou aktualizaci; všech deset prošlo 5. 10. 2026 a
záměna výsledku detekce kódování (mutace) shodila čtyři kontroly.

Od 5. 10. 2026 je i jádro CPU 80C188 (`src/i86/`, `src/cpu80188.*`,
podrobnosti v 6.12) s testy `cpu_selftest` (bez dat) a `cpu_test` ve
dvou bězích nad daty SingleStepTests; celkem 13 PASS.

Plán fází: 0 kostra
repozitáře, 1 jádro CPU a rozbalení PKLITE, 2 reverzní inženýrství
(rozhodovací brána pro 6.10 a 6.11), 3 bezhlavý stroj nabootuje do
MAINMENU, 4 ADSP-2105 a zvuk, 5 aplikace a doplněk NVDA, 6 CI a vydání.

## 6. Otevřené otázky

### 6.1 Rozhraní boot ROM, které je nutné nahradit HLE

Boot ROM chybí a fyzická Aria není (uživatel, 5. 10. 2026). Co
poskytovala, se odvodí z tabulky poskytovatelů a konzumentů přerušení
v pořadí načítání CONFIG.SYS a AUTOEXEC: vektor, který někdo volá dřív,
než ho kdokoli nainstaluje, je služba boot ROM.

API.DOC zmiňuje „Boot & Flash ROM“ (SpeakChecksum, SpeakVersion), tedy
boot ROM má vlastní číslo verze a kontrolní součet.

**Oprava 5. 10. 2026: `INT 69h` služba boot ROM není.** První tvrzení
(„volá ho VOICE.SYS a nikdo ho neinstaluje“) stálo na hledání jen tvaru
`mov ax,2569h`. `tools/ints.py` najde i `mov ah,25h` + `mov al,69h` a
ukázal, že **`VOICE.SYS` ho sám instaluje** (`VOICE.SYS+01ECh`–`01F0h`,
obsluha na `01ADh`, větví podle `CH` = 0 až 5). Ostatní uživatelé
(`DLFFSA`, `RAMDISK`, `TRICKSS2`) se načítají až po něm.

Co VOICE.SYS při startu dělá (`VOICE.SYS+01CEh`–`0244h`, řetězce
„Voice device rev 1.3. RWC 18-Dec-97“, „Patched FLASHDSK.SYS
successfully“, „Could not find FLASHSK.SYS to patch.“):

1. Nastaví `INT 07h` na obsluhu, která jen posune návratovou adresu o 2
   (`01C5h`). Na 80188 je INT 7 past na instrukce ESC, takže jde nejspíš
   o přeskakování instrukcí koprocesoru **[předpoklad]**.
2. Nastaví `INT 69h`.
3. Hledá v segmentech `0262h`, `0263h`, `0264h` slovo `0580h` na offsetu
   `027Ah`, tedy rezidentní **`FLASHDSK.SYS` — ovladač disku B: z boot
   ROM** zavedený v RAM. Kde ho najde, přepíše 10 bajtů na `027Ah-7` (cíl
   `ES:0273h`) kódem `mov dl,al; mov dh,0; mov ch,2; int 69h; pop bp;
   ret` (`0247h`). Zápis ovladače do portu `0580h` tak jde přes
   `INT 69h` funkci 2.
4. Když ho nenajde, jen to oznámí přes `INT 10h AH=0Eh` a pokračuje.

Z toho plyne: port `0580h` je **sdílený latch**, který používá stránkování
flash i hlasový hardware, a VOICE.SYS je jeho správce se stínovou kopií.
To je stejný vzor jako latche Eureky. Boot ROM tedy obsahovala ovladač
`FLASHDSK.SYS` a zaváděla ho na nízké segmenty kolem `0262h` (HANDOFF
6.2, 6.10, 6.11).

Kandidáti na služby boot ROM podle `tools/ints.py` (7.05, jen
nekomprimované programy, PKLITE zatím přeskočeny):

- BIOS: `10h` (`AH=0Eh` výpis, volají skoro všichni), `11h`, `13h` (před
  BACKPACK), `15h` (RAMDISK), `16h` (dřív, než ho převezme BKB), `17h`,
  `19h` (TRICKSS2), `1Ah`.
- Jádro DOS: `20h`–`2Fh`, také z boot ROM.
- Neznámé, s instalací nikde nenalezenou: `63h` (BKB), `68h` (BACKPACK),
  `6Bh` (BPHDARIA), `2Dh` (TRICKSS2). Mohou je instalovat PKLITE
  programy (AAPI), nebo je mohou poskytovat boot ROM; rozhodne fáze 1.
- `60h`, `61h`, `62h` jsou dokumentované (AAPI, SPEAK). Instalace nebyla
  vidět (AAPI je PKLITE, SPEAK nejspíš zapisuje do tabulky vektorů
  přímo).

### 6.2 Mapa paměti a stránkování

1 MB NV RAM a 2 MB flash se nevejdou do 1MB adresního prostoru 80C188,
musí se stránkovat. Kandidáti: chip selecty PCB (`RAMDISK.EXE` zapisuje
port `00A0h` = UMCS při PCB na 0), porty `0480h`, `0580h`, `0702h`.
`0580h` je sdílený latch, do kterého zapisoval ovladač flash z boot ROM
(`FLASHDSK.SYS`) a který po startu spravuje `VOICE.SYS` přes `INT 69h`
(viz 6.1).

### 6.3 Rozhraní DSP

`SPEAK.EXE` programuje DMA kanál 0 (`C0h`–`CAh`) a zapisuje porty `0401h`
a `0402h`. Neznámé: jak se do ADSP-2105 nahraje `LPC.DSP` (2105 nemá
host port, bootuje z byte-wide boot paměti), jak se předávají rámce a kam
vede zvukový výstup (sériový port DSP → DAC?).

### 6.4 Klávesnice

`BKB.COM` čte `0700h` (5×), `0480h`, `050Dh`, `0581h`, `0584h`; instaluje
`INT 13h` a `INT 16h`. Rozložení braillských kláves zatím neznámé.

### 6.5 RTC a buzení alarmem

API má `AwokenByRTC` a `EnableAlarm`; `ARIALARM.EXE` a `ARIAMODE.EXE`
přebírají `INT 08h` a `INT 1Bh`. Port RTC zatím neznámý.

### 6.6 Sériový port

`FOSSIL.COM` přebírá `INT 14h` a používá porty `0600h`, `0601h`, `0681h`
a registry PCB `INT0`/`INT1` (`38h`, `3Ah`) a časovač 1 (`5Ah`, `5Eh`).

### 6.7 Paralelní port a Backpack

`BACKPACK.SYS` přebírá `INT 13h`, `BPHDARIA.SYS` volá `INT 6Bh`.

### 6.8 PCMCIA

`CARDMAN.EXE` (PKLITE); `TRICKSS2.EXE` čte `057Fh`, `ATKBD.COM` `057Eh`.

### 6.9 Napájení

`TURNOFF.COM` (16 B): `cli; mov dx,0701h; mov al,0; out dx,al; jmp $` —
port `0701h` vypíná. Stav baterie zatím neznámý.

### 6.10 Rozhodnutí: vrstva DOS

Buď jádro FreeDOS na HLE BIOSu, nebo `INT 21h` v hostiteli. Riziko: ovladače
(`DLFFSA`, `BACKPACK`, `BPHDARIA`) a `COMMAND.COM` z ROM-DOS mohou záviset
na vnitřnostech ROM-DOS (List of Lists, DPB, řetěz ovladačů). Rozhodne
fáze 2.

### 6.11 Rozhodnutí: vrstva flash

Buď skutečná emulace flash čipu a jeho okna (nutné, pokud zápis `DLFFSA`
jde přes `0702h`/`0480h`), nebo disk B: jako HLE blokové zařízení.
Rozhodne fáze 2.

### 6.12 Jádro CPU 80C188 a jeho ověření

**Varianta čipu.** Aria má klasický 80C188 (ne 80C188EB): `SPEAK.EXE`
programuje DMA kanál 0 na `C0h`–`CAh` a varianta EB žádné DMA nemá a PCB
má jinak rozložené (`SPEAK.EXE+09F0h`, `+0A1Dh`). Emulovat se tedy má
PCB klasického 80186/80188: řadič přerušení, tři časovače, dva kanály
DMA, chip selecty a relokační registr.

**Licence převzatelného kódu, ověřeno 5. 10. 2026 v hlavičkách souborů
na GitHubu mamedev/mame (větev master):**

- `src/devices/cpu/i86/i86.cpp` — BSD-3-Clause, autor „Carl“, „NEC
  V20/V30/V33 emulator modified back to a 8086/80186 emulator“.
- `src/devices/cpu/i86/i186.cpp` — BSD-3-Clause, „Carl“, periferie PCB
  podle ovladače rmnimbus (Phill Harvey-Smith) a zvuku Leland (Aaron
  Giles, Paul Leaman); obsahuje i variantu `i80188`.
- `src/devices/cpu/adsp2100/adsp2100.cpp` a `2100ops.hxx` — BSD-3-Clause,
  Aaron Giles; rodina ADSP-21xx včetně 2105 (pro fázi 4).

BSD-3 je s MIT projektu slučitelná, jen se musí zachovat upozornění
(jako `src/LICENSE.superzazu-z80.txt` v Eurece).

**Testovací sady, ověřeno 5. 10. 2026 přes GitHub API
(github.com/SingleStepTests):**

- `8088` — MIT, generovaná na skutečném čipu (AMD D8088, NMOS), verze 2,
  10 000 testů na opkód, stav registrů a paměti před a po. Formát JSON
  i binární `MOO` (gzip); `v2_binary` má 324 souborů, celkem 505 MB.
  Vzorový parser MOO v C je v `tools/moo_parser.c`.
- `v20` — MIT, NEC V20 (μPD70108), `v1_native` 362 souborů JSON.gz,
  883 MB. V20 má instrukce 80186 (`60h`–`62h`, `68h`–`6Fh`, `C0h`/`C1h`,
  `C8h`/`C9h`), takže z ní jde ověřit rozšíření 80186; testy s prefixy
  REPC a REPNC (jen V20) se vyřazují. Podmnožina pro rozšíření 80186 má
  řádově 50 MB.
- `8086` a `80286` existují také (MIT); 80188 se sběrnicí podobá 8088.

Žádná sada pro samotný 80186/80188 není. Známé rozdíly 80186 proti 8088,
které se proto musí u testů 8088 vyřadit **a vypsat**, ne potichu:
maskování počtu posunů na 5 bitů, past `INT 6` na nedefinované opkódy,
návratová adresa po chybě dělení, aliasy opkódů, které 80186 nemá.
Kde se V20 od 80186 liší (nedefinované příznaky), totéž.

Ověřeno 5. 10. 2026 v kódu MAME (`i186.cpp` na commitu `2aa75c50`), ještě
ne proti datasheetu Intelu:

- posuny `C0h`/`C1h`/`D2h`/`D3h` maskují počet `& 0x1f` (řádky 409,
  434, 488, 513);
- neplatný opkód vyvolá `INT 6` (řádek 669), `mov cs,…` (`8Eh` s reg=1)
  také (řádek 399);
- instrukce ESC (`D8h`–`DFh`) vyvolají `INT 7`, když je nastaven bit 15
  relokačního registru PCB, a IP ukazuje zpět na ESC (řádky 540–545).
  **To potvrzuje výklad obsluhy INT 7 ve `VOICE.SYS` (6.1):** posune
  návrat o 2 bajty, tedy přeskočí opkód ESC a modrm, a programy přeložené
  s emulací koprocesoru tak na Arii bez 8087 nespadnou. Bajty posunutí za
  modrm obsluha nepřeskakuje; zda to vadí, ukáže až běh.
- Chyba dělení: MAME ukládá adresu **za** instrukcí na 8086 i na 80186
  (`i86.cpp`, `interrupt(0)` po `m_ip` za instrukcí). Jak to dělá
  skutečný 80186, zůstává neověřené; pro programy DOSu to sotva hraje
  roli.

Ostatní body (aliasy opkódů, nedefinované příznaky V20) se ověří až na
datech testů.

**Formát MOO** (podle `v2_binary/README.md` a `tools/moo_parser.c`
v repozitáři 8088): soubor je gzip; uvnitř bloky `typ (4 B ASCII) + délka
(uint32 LE) + data`. Hlavička `MOO ` (verze, počet testů, jméno CPU), pak
bloky `TEST` (index uint32 a podbloky `NAME`, `BYTS`, `INIT`, `FINA`,
`CYCL`, `HASH`). `INIT`/`FINA` obsahují `REGS` (maska uint16 a hodnoty
uint16 v pořadí bitů **ax, bx, cx, dx, cs, ss, ds, es, sp, bp, si, di,
ip, flags**), `RAM ` (počet uint32, položky adresa uint32 + bajt) a
`QUEU`. `FINA` nese jen změněné registry a paměť. Neznámé bloky se mají
přeskakovat podle délky. Gzip rozbalí náš `Inflate` (`src/zip_reader.*`),
stačí přidat hlavičku gzip. Fronta instrukcí a takty (`QUEU`, `CYCL`) se
pro náš cíl ignorují. Sada V20 je jen v JSON.gz, takže pro ni bude potřeba
malý čtecí JSON, nebo převod nástrojem `tools/convert_binary.py` z jejich
repozitáře (mimo náš strom).

Data testů do repozitáře nepatří (jsou velká, i když MIT); budou mimo
strom v proměnné `ARIA_CPUTESTS`, podobně jako ZEXDOC v Eurece, a test
jádra bez nich ohlásí přeskočení.

**Rozhodnuto uživatelem 5. 10. 2026:** jádro se převezme z MAME (níže) a
data testů leží v `D:\aria\cputests` (`ARIA_CPUTESTS`): `8088\v2_binary`
celé a z `v20\v1_native` jen opkódy rozšíření 80186. Staženo 5. 10. 2026:
324 souborů 8088 a 29 souborů V20 (`60h`–`62h`, `68h`–`6Fh`, `C0h`,
`C1h` po podopkódech, `C8h`, `C9h`), celkem 562 MB, s README a LICENSE;
velikost každého souboru sedí s GitHub API a všechny projdou `gzip -t`.

Původní návrh: převzít jádro i86/i186 z MAME jako
samostatnou třídu bez rámce MAME (paměť a porty přes zpětná volání, bez
debuggeru a ukládání stavu) a periferie PCB z `i186.cpp`. Je odladěné na
mnoha strojích a už obsahuje chování 80186. Zpětné volání do HLE boot ROM
se udělá pastí na vyhrazený opkód, jako TRAP v Eurece. Přesnost na takty
není cíl: zvuk dělá DSP, CPU určuje jen rychlost a časovače.

**Výsledek 5. 10. 2026: jádro je převzaté a projde oběma sadami.**

Kde je co:

- `src/i86/` — kód MAME (`LICENSE.mame-i86.txt`, BSD-3). `i86inline.h`
  je doslovná kopie; `i86.cpp` a `i186.cpp` jsou poskládané z doslovných
  úseků originálu po řádcích (značky „verbatim from … lines …“) a každá
  naše změna nese komentář `ARIA:`. `i86.h` je náš: deklaruje třídy pod
  jmény MAME a nahrazuje rámec MAME malými náhradami (adresní prostor nad
  `I86Bus`, logování, háčky debuggeru).
- `src/cpu80188.*` — rozhraní pro emulátor: `Step()` (celá instrukce
  i s prefixy a REP), `Run()`, registry, linky INTR a NMI.
- Periferie PCB **od 5. 10. 2026 převzaté** (`i186.cpp` řádky 927–2099
  doslovně): řadič přerušení (master i iRMX), tři časovače, dva kanály
  DMA, chip selecty, relokační registr. Plánovač MAME (`emu_timer`) je
  nahrazen časovači v taktech CPU, které se obsluhují mezi instrukcemi
  (`ServiceTimers`) a při expiraci čtou čas splatnosti, takže periodický
  časovač neujíždí; HLT přeskočí rovnou na příští událost časovače
  (`HaltUntilTimer`). Piny desky jsou v `Cpu80188`: `SetInterruptPin`
  (INT0–INT3 do řadiče; INTR CPU budí jen řadič), `SetNmiLine`,
  `SetDmaRequest`, `SetTimerInput`; výstupy TMROUT a zápisy chip selectů
  jdou do `I86Bus::TimerOutput` a `ChipSelect` (pro mapu paměti, 6.2).

Chyby MAME, které testy z hardwaru našly a které jsou opravené (vše
s `ARIA:` v kódu):

1. Slovo na offsetu `FFFFh` bere druhý bajt z offsetu `0000h` téhož
   segmentu, ne z další fyzické adresy (`m_ea_base` v `i86.h`).
2. `JL` testoval `SF!=OF && !ZF`, správně jen `SF!=OF`.
3. `IMUL` bajt i slovo: CF=OF podle toho, zda horní polovina je
   znaménkové rozšíření dolní, ne zda je nulová.
4. `DAA` a `DAS`: druhý krok podle starého AL (hranice `9Fh` při AF,
   jinak `99h`) a starého CF, jak to dělá 8088 (liší se jen pro vstup,
   který není BCD).
5. `AAA` a `AAS`: AH jen ±1 podle dokumentace Intelu pro 86/88/186/188;
   MAME měl variantu 80286 (`AX += 106h`). Na 80186 nezměřeno.
6. `AAM`: SF, ZF a PF z AL, ne z AX.
7. OF po posunu a rotaci o CL nebo o přímou hodnotu (`C0h`, `C1h`,
   `D2h`, `D3h`) MAME na 80186 nenastavoval vůbec, ani pro počet 1;
   teď pravidlo jednobitového kroku na poslední krok jako 8088
   (`SetShiftOverflow`). Pro počet nad 1 je OF podle Intelu nedefinovaný.
8. `PUSH SP` ve tvaru `FF /6` ukládá už snížené SP jako krátký tvar `54h`.
9. `82h` (bajtový alias `80h`) znaménkově rozšiřoval zdroj na 32 bitů a
   kazil CF/AF.
10. `ENTER` přepsán podle algoritmu Intelu (displej z *původního* BP, pak
    ukazatel rámce, až potom odečtení velikosti; úroveň mod 32).

S periferiemi přibyly tři změny, které našel `cpu_selftest`, ne sady
z hardwaru (ty časovače nepokrývají):

11. `HLT` nevynuluje zbytek časového úseku (`m_icount = 0`); s plánovačem
    MAME to nevadilo, u nás by časovač zaostal o celý úsek (v `Step()`
    16M taktů) a přerušení by se slila do jednoho.
12. Odstraněno přeskakování taktů u `jmp $` (idle smyčka): posouvalo
    přerušení časovačů a přenosy DMA až do dalšího úseku.
13. Reset zastaví běžící časovače; MAME je nechával naplánované.

Scénáře `cpu_selftest` pro periferie, všechny s PCB přesunutým na
`0000h` jako na Arii: INT0 dvakrát přes řadič s EOI mezi tím (bez EOI
druhé přerušení nepřijde), časovač 0 periodicky po 400 taktech během
HLT, DMA paměť→paměť a DMA paměť→port `0401h` synchronizované DRQ0, které
se zastaví na koncovém počtu (tvar, jaký programuje `SPEAK.EXE`). Mutace
(zrušené EOI pro INT0, poloviční perioda časovače) shodí příslušný
scénář.

Co zatím **není ověřené** proti skutečnému kódu Arie: zda AAPI/FOSSIL/
SPEAK programují PCB tak, jak model MAME čeká (úrovňové vs. hranové
INT0–3, kaskáda, prescaler časovače 2). To ukáže až běh ovladačů
(fáze 3).

Co testy z rozhodnutí nesrovnávají, vypisuje `tests/cpu_test.cpp` i
s počty (8088: 46 375 testů a 28 souborů; V20: 43 508 testů):

- opkódy, které 80186 předefinoval (`60h`–`6Fh`, `C0h`, `C1h`, `C8h`,
  `C9h` jsou na 8088 aliasy), stav „undefined“, „prefix“, „fpu“;
- posuny o CL ≥ 32 (8088 nemaskuje) a v sadě V20 přímé počty ≥ 32 (V20
  nemaskuje, 80186 ano);
- `IN`/`OUT`/`INS`/`OUTS` na portech `FF00h`–`FFFFh` (PCB po resetu);
- `MOV Sreg` s polem reg 1 (CS) a 4–7 (u 80186 v MAME `INT 6`, na
  hardwaru neověřeno);
- REP před `IDIV` (8088 neguje podíl, artefakt mikrokódu);
- po chybě dělení aritmetické příznaky v FLAGS i v kopii na zásobníku
  (zbytky mikrokódu dělení);
- prefixy REPC/REPNC (jen V20).

Nedokumentované a aliasové tvary se srovnávají jen pro informaci:
selhávají `SETMO`/`SETMOC` (`D0h`–`D3h` /6) a `FF /7`, což jsou
vlastnosti 8088; ostatní aliasy (`82h`, `F6h /1`, `F7h /1`) projdou.

Čísla: 8088 2 535 686 srovnání, 0 selhání; V20 (rozšíření 80186) 127 545
srovnání, 0 selhání. Mutace (vypnutá past ESC, vrácený JL, vrácené
zalomení segmentu) shodí vždy příslušný test. `tests/cpu_selftest.cpp`
bez dat ověřuje reset, přesun PCB relokačním registrem, INT, INTR a past
ESC s obsluhou zkopírovanou z `VOICE.SYS+01C5h`.

**Otevřené k 6.12:** rozbalení PKLITE (zbytek fáze 1). Kmitočet CPU
Arie není známý (potřebný pro převod taktů na reálný čas a pro časovače),
patří k 6.5 a fázi 3.

## 8. Pořadí

1. Fáze 0: kostra repozitáře — **hotová** 5. 10. 2026: dokumenty,
   Makefile, skripty pro WSL i dávky `.bat`, `.clangd`, načtení disku B:,
   analytické nástroje v `tools/` a beads.
2. Fáze 1: jádro 80188 **hotové** 5. 10. 2026 (6.12); zbývá rozbalení
   PKLITE během vlastního stubu programů a pak znovu sken portů jen
   v kódových segmentech. Periferie PCB patří k fázi 3, ale dají se
   převzít z `i186.cpp` kdykoli dřív.
3. Fáze 2: 6.1 a 6.2 jako první, protože na nich stojí všechno ostatní.
