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

## 8. Pořadí

1. Fáze 0: kostra repozitáře — **hotová** 5. 10. 2026: dokumenty,
   Makefile, skripty pro WSL i dávky `.bat`, `.clangd`, načtení disku B:,
   analytické nástroje v `tools/` a beads.
2. Fáze 1: jádro 80188 a rozbalení PKLITE; pak znovu sken portů jen
   v kódových segmentech.
3. Fáze 2: 6.1 a 6.2 jako první, protože na nich stojí všechno ostatní.
