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

### Popis stroje z roku 1997 (Wayback Machine)

Stránka „guided tour“ Robotronu z 2. 4. 1997
(`web.archive.org/web/19970402142428/http://www.robotron.net.au:80/aria.htm`)
je nejúplnější dochovaný popis; fotky `aria1.gif`, `aria3.gif` (1997),
`aria5.jpg`, `aria200.jpg` (dnešní stránka). Do repozitáře se
nekopírují, jen fakta vlastními slovy:

- **Rozměry a vzhled:** 222 × 102 × 28 mm, 550 g; uhlově šedý, sametový
  protiskluzový povrch, **funkční a kurzorové klávesy růžové**, **braillské
  perleťové**. Rozložení Perkins: šest bodů a mezerník uprostřed, nad nimi
  řada osmi funkčních kláves, vpravo nahoře křížová skupina — **střed je
  Shift, kolem šipky** (potvrzuje vzpomínku uživatele, 6.4). Vlevo nahoře
  nápis „ARIA“ s pruhy. Displej žádný.
- **Zapnutí:** tlačítko zapuštěné v **levém boku** (on/reset). Pípne,
  několik sekund startuje, pak zazní **harfa** (`SOUNDS\HARPUP.04`), která
  ohlašuje hlavní nabídku. **Vypnutí: všechny čtyři šipky naráz v hlavní
  nabídce**, Aria řekne „off“. Na baterii se po nečinnosti vypne sama,
  s výstražným zvukem.
- **Konektory:** zespodu braillské popisky. Vzadu zleva **DC zdířka**,
  **subminiaturní 9pinový sériový port**, **subminiaturní paralelní
  port** (oba přes redukce, 6.7); na pravém boku **mono sluchátka** a
  **slot karty** s vysouvacím tlačítkem (karty do 64 MB plánované, do
  20 MB existující).
- **Napájení:** vnitřní NiCd (nabití z vybité za ~3 h, den provozu),
  samočinně přepínaný adaptér 110/220 V, nabíječ hlásí stav (6.4, `0700h`);
  **hodiny mají vlastní lithiovou baterii na 10 let** (6.5).
- **Paměť podle stránky:** 1 MB RAM, „RAMdisk is 1 megabyte“, DOS ve 2MB
  flash. **Rozpor:** VDISK má výchozích 319 KB (6.2) — buď marketingové
  zjednodušení, nebo jiná konfigurace; otevřené.
  **Rozhodnuto 6. 10. 2026 podle uživatele:** platí 319 KB; „1 MB“ je
  marketingový údaj o celé RAM — tolik volného místa na RAM disku nikdy
  nebylo.
- **Funkční klávesy v hlavní nabídce:** F1 textový editor, F2 hodiny a
  stopky (alarmy, druhé časové pásmo, odbíjení), F3 kalkulačka (virtuální
  obrazovka), F4 komunikace (adresář, modem, **emulace DECtalk** — Aria
  jako hlasový syntetizér pro jiný počítač), F5 telefonní seznam (tónová
  volba reproduktorem), F6 diář a kalendář (**Aria se probudí a ohlásí
  schůzku** — `AwokenByRTC`, 6.5), F7 správce souborů, F8 spuštění
  programu. Se Shiftem: Shift+F1 datum, Shift+F2 čas, **Shift+F7
  překladač braillských pravidel** (`LOUIS.EXE`), **Shift+F8 DOS**
  (`RUNDOS.EXE`). Volby (Mode) jsou dostupné odevšad; schránka mezi
  funkcemi.
- **Aktualizace:** flash se programuje sama, novou verzi stačí přenést
  z jiného počítače (sériově) nebo z karty. `ariaware.htm` (1997) nabízel
  starší sestavení: anglické 0627, thajské 0622, **české 0621** (12. 12.
  1996), na `ftp.robotron.net.au/ariaware/…`.
  **Úplný seznam zveřejněných sestavení** podle deseti verzí stránky
  `ariaware.htm` v archivu (4/1997–9/2000): česky 0621, 0648, 0687, 0705;
  anglicky 0627, 0647, 0662, 0689, 0704; thajsky 0622, 0649, 0688.
  Uživatel má 0687, 0705, 0704, 0690 a 0699; 0690 a 0699 na stránce
  nikdy nebyly. **Ostatní se nedochovaly** (ověřeno 6. 10. 2026): Wayback
  Machine má jen stránky, soubory z FTP vrací 404, a dnešní FTP server
  (stejný hosting jako web) má od 10/2024 prázdnou anonymní oblast a na
  všechny cesty odpoví 550.
- **Uživatelská příručka** existovala tištěná, zvuková a v Braillu;
  nikde nenalezena (vyhledávání 6. 10. 2026).

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

Počet 13 už neplatí: od 6. 10. 2026 přibyl `pklite_unpack --check` a pak
`boot_test`, celkem **15 PASS**. Fáze 3 začala 6. 10. 2026 (`ar-4cu`):
stroj `src/aria_machine.*` (dekódování paměti s oknem D000h, latche,
RTC `src/rtc146818.*`, matice kláves, baterie, vypnutí) a BIOS
emulátoru `src/hle_bios.*` (podrobnosti v 6.10 a 6.11). Na něm
nabootuje jádro FreeDOS z disku B: české 7.05 až po svůj úvodní text;
dál zatím ne, viz 6.10.

**Okno podle Eureky, 8. 10. 2026** (požadavek uživatele: převzít okno,
nabídku, nastavení, F11/F12 z emulátoru Eureky). Krok 1: `src/win/window.*`
beze změny z Eureky, `src/main_window.*` (směrování kláves, tóny, titulek),
`src/res/aria.rc` (nabídka česky, dvě tabulky akcelerátorů, VERSIONINFO,
manifest), verze z `git describe` (`build/aria_version.h`). Nabídka má
jen to, co už funguje: Konec, uvolnění klávesnice, F11, nápověda.
Odchylka od Eureky: Alt+F4 zavírá okno vždy (Aria Alt nemá); Alt a F10
nabídku neotevřou. Další krok: zapnutí a vypnutí (`0701h`, akord čtyř
šipek „konec“ podle uživatele), teplý reset, odpojení baterie, nastavení
a uložení paměti mezi spuštěními (`ar-ram`). Doplněk NVDA zatím ne.

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

**Neplatí od 6. 10. 2026:** že `0580h` sdílí i hlasový hardware, kód
nepodpořil. Rozbor celé obsluhy `INT 69h` (6.2) ukázal, že latch vybírá
jen režim a stránku okna D000h (flash, karta, vypnuto). Proč je správce
okna ve `VOICE.SYS`, kód neříká; nejspíš proto, že ten ovladač se
načítá první a záplatu `FLASHDSK.SYS` potřebuje dřív, než kdokoli jiný
okno použije.

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

**Doplněno 6. 10. 2026 s rozbalenými programy PKLITE** (`bin/pklite_unpack`,
6.12; `tools/ints.py` s `ARIA_UNPACKED`, sken EXE jen pod počátečním SS,
tedy v kódu). Výsledek pro 7.05, ověřený pohledem do `tools/dis.py`:

- **Kandidáti na služby boot ROM, se skutečným voláním a bez instalace
  na B:** (hledáno i přímým zápisem do tabulky vektorů na `18Ch`, `194h`,
  `1A0h`, `1ACh` — nenalezeno, jen vlastní proměnné programů):
  - `INT 63h` — `BKB.COM` ×5, funkce `AH=2` (`BKB.COM+158F`, výsledek se
    vypisuje jako číslice) a `AH=3` (`+15A5`);
  - ~~`INT 65h` — `AAPI.EXE+DBF0`, `AX = (arg << 8) + arg2`~~ —
    **opraveno 6. 10. 2026: instaluje ho `ARIALARM.EXE`** přes `setvect`
    Borlandu (sken to nevidí). Doklad je odinstalace
    `ARIALARM.EXE+21A8h`–`+21E1h`: ověří, že `INT 1Ch` ukazuje na
    `+211Ah` a `INT 65h` na `+214Dh`, a obnoví je. Obsluha `+214Dh`
    uloží `AH` (číslo alarmu) a `AL != 0` (zapnout) a nastaví požadavek;
    to je `EnableAlarm` z API, AAPI ho jen předává;
  - `INT 68h` — `BACKPACK.SYS+0B07` a `+38FF` (v `DL` číslo disku 0/1),
    `FORMAT.EXE+1A5C`;
  - `INT 6Bh` — `BPHDARIA.SYS` ×13 (`+0270`: `AH` a `DL` z proměnných).

  Po opravě `65h` tedy zbývají tři kandidáti: `63h`, `68h`, `6Bh`.

  **Oprava 6. 10. 2026: ani `INT 63h` není služba boot ROM.** Je to
  **MODE API** podle `ARIA-INT.H` (`MODE_INT 0x63`: 0 popup, 1 nastav
  stupeň Braillu, 2 nastav hlasitost, 3 zjisti hlasitost, 4 zjisti
  předvolbu vytáčení); poskytuje ho `ARIAMODE.EXE` (`setvect`, sken to
  nevidí). BKB volá `AH=2` a `AH=3` = hlasitost. Hlavička také potvrzuje
  `ALARM_INT 0x65` (0 budík, 1–3 alarmy diáře). **Zbývají dva kandidáti,
  `68h` a `6Bh`, a oba používají jen ovladače mechanik Backpack** — je tedy
  možné, že patří jim, ne boot ROM (ověřit).

  **Ověřeno 6. 10. 2026: patří jim.** Oba zapisují do tabulky vektorů
  přímo (`ES = 0`, sken přes `INT 21h` to nevidí): `BACKPACK.SYS+318Bh`
  nastaví `INT 68h = CS:2A10h` (a `INT 1Eh`, tabulku parametrů diskety,
  na `CS:2B6Eh`), `BPHDARIA.SYS+1935h` nastaví `INT 6Bh = CS:0C39h`.
  Jsou to ovladače Micro Solutions upravené pro Arii: „BP v2.11.AR“
  (1996, Ron Proesel) a „HD v2.03.DZ“ (BPHDDRV.SYS, 1998, s hesly disků
  a hláškami anglicky, německy, francouzsky).

**Závěr k 6.1 (6. 10. 2026): boot ROM nemá žádnou službu specifickou pro
Arii.** Všechny vektory nad `2Fh` instalují programy z B: (60h AAPI, 61h
SPEAK, 62h AAPI, 63h ARIAMODE, 64h BKB, 65h ARIALARM, 66h tisk, 68h
BACKPACK, 69h VOICE.SYS, 6Ah BKB, 6Bh BPHDARIA). Boot ROM tedy musí
poskytnout:

- **BIOS** (volá ho kód z B:): `10h` (výpis `AH=0Eh` — AAPI ho zachytává
  do řeči — a funkce videa pro virtuální obrazovku), `11h` (vybavení),
  **`12h` (velikost paměti — čte AAPI, takže z ní plyne, kolik RAM vidí
  DOS, 6.2)**, `13h` (disky; BACKPACK ji řetězí, volají CARDMAN, DLFMT,
  FORMAT), `14h` (sériový port, než ho převezme FOSSIL), `15h` (RAMDISK,
  DLFMT), `16h` (klávesnice, než ji převezme BKB: VOICE.SYS, DLFFSA,
  BACKPACK, BPHDARIA, TRICKSS2), `17h` (tiskárna), `19h` (restart),
  `1Ah` (čas; téměř všechny programy), tabulky `1Dh`–`1Fh`, a hardwarové
  vektory, které TSR řetězí: `08h` (tik), `09h`, `1Bh`, `1Ch`.
- **Jádro DOS:** `20h`, `21h`, `25h`/`26h`, `28h`, `2Fh` (multiplex),
  a `2Dh` (AMIS, TRICKSS2). COMMAND.COM z B: je ROM-DOS (6.10).
- **`FLASHDSK.SYS`** (disk B:, záplatovaný VOICE.SYS) a **ROM disk A:**.
- **Data:** bajt na `FFFF:000Eh`, verze boot ROM a sériové číslo pro
  `GetAriaVersion`, kontrolní součet pro `SpeakChecksum` (6.5, 6.14).

Jednotlivé výskyty `01h`, `02h`, `0Ah` (CALC, ARIACLK) jsou z prostředí
Borland pro plovoucí čárku nebo šum.
- **BIOS a DOS** (rozsah `00h`–`2Fh`): navíc proti dřívějšku `01h`,
  `02h`, `0Ah`, `12h`, `14h`, `28h`, `2Fh`, `1Dh`–`1Fh` (tabulky BIOSu)
  z aplikací. Jádro DOS je celé v boot ROM.
- **Ne boot ROM:**
  - `INT 34h`–`3Eh` (CALC, ARIACLK, COMMS, AAPI ×1) — emulace koprocesoru
    běhového prostředí Borland; instaluje ji runtime sám ve smyčce, proto
    ji sken nevidí;
  - `INT 64h` — instaluje `BKB.COM`, AAPI ji volá až za běhu
    (`AAPI.EXE+A726`), takže „použito před instalací“ je jen artefakt
    pořadí načtení;
  - `INT 69h` — `VOICE.SYS` (výše), používá i AAPI ×6.
- **Šum**: jednotlivá „volání“ `55h`, `6Eh`, `72h`, `77h`, `84h`, `88h`,
  `94h`, `A5h`, `AEh`, `B4h`, `BFh`, `C3h`, `CAh`, `D6h`, `FFh` jsou data
  uvnitř kódových segmentů (např. `AAPI.EXE+14ABC` leží v textu).

Pořadí podle načtení je u rezidentních programů jen orientační: TSR volá
služby až za běhu, ne při instalaci.

**BIOS emulátoru, první verze 6. 10. 2026** (`src/hle_bios.*`). Pahýl ROM
leží v horním 1 KB (`FFC00h`), který zůstává ROM při každé velikosti
UCS, i po `UMCS = FFF8h` z RAMDISK.EXE (6.2). Každý vektor služby ukazuje
na `STI; 0Fh nn; IRET`; opkód `0Fh` je na 80186 neplatný, takže ho
žádný skutečný program nevykoná, a jádro (`i186.cpp`, komentář `ARIA:`)
ho s nastavenou obsluhou předá do C++ s číslem `nn`. Čekající volání
(`INT 16h` bez klávesy, `INT 19h`) vrátí IP na `STI`, takže se opakuje
s povolenými přerušeními a tik běží dál. Obsluhy: `10h` (textová
obrazovka 80×25, kterou nikdo nevidí, a záznam výstupu `AH=0Eh`), `11h`,
`12h` (640 KB), `13h` (6.11), `14h` (bez linky, dokud ji nepřevezme
FOSSIL), `15h` (jen `88h`, `90h`, `91h`), `16h` (fronta, dokud ji
nepřevezme BKB), `17h` (tiskárna „připravena“), `19h`, `1Ah` (tiky a RTC
z 6.5), `08h` (tik, `INT 1Ch`, EOI typu 8 na `0022h`), tabulka disket
`1Eh`. Ostatní vektory vedou na `IRET`.

Předpoklady, které nic na B: nedokládá, a proto se mají ověřit:

- **takt CPU 8 MHz** (`AriaMachine::kCpuHz`, 6.12): časovač 2 dělí 61,
  časovač 0 počítá 1 801, takže tik je 18,2 Hz jen při tomto taktu;

  **Neplatí od 7. 10. 2026:** časovač 2 tik dělat nesmí. AAPI si ho
  bere: hák `INT 13h` na `AAPI.EXE+9F20` před každým voláním disku
  nahraje jeho maximum (`out 62h`) a po něm čte jeho stav (`in 60h`),
  měří tak délku diskových operací. *(Výklad „měří disk“ týž den
  opraven v 6.4: `INT 13h` je na Arii přerušení časovače 2, ve kterém BKB
  snímá klávesnici, a hák AAPI nastavuje jeho periodu.)* Na Arii šlo B: přes okno a
  `FLASHDSK.SYS`, takže hák běžel zřídka; u nás každý sektor B: jde
  přes `INT 13h`, hák maximum přepsal na `C925h` a tik se zastavil
  (zachyceno krokováním: zápis na `3189:0015` po `INT 13h` jádra).
  Časovač 1 je FOSSILu (`FOSSIL.COM+07EB`). Zpožďovací smyčka
  `AAPI.EXE+10B35` čte stav **časovače 0** a čeká, až překročí `753h`,
  takže jeho maximum je větší. Tik je proto jen z časovače 0 s maximem
  65 536, a aby to bylo 18,2 Hz, předpokládá se **takt 4,77 MHz** (čítač
  1,19318 MHz jako časovač PC). Je to odvození, ne doklad;

  **Doplněno 7. 10. 2026: tik přichází přes INT2, ne jako přerušení
  časovače.** SPEAK přebírá `INT 08h` a uvnitř syntetizuje řeč se
  zapnutými přerušeními (rezidentně `8A6Ch` → `8A05h` → `7FF0h`), kde
  čeká na své přerušení DMA0 (`62A0h`); to má vždy prioritu 7
  (`SPEAK.EXE+0F86h`, `+62BBh`, `+780Eh`). Přerušení časovače, které je
  ještě obsluhované, by ho podle pravidel 80186 (a MAME) zablokovalo
  napořád, ať má časovač prioritu jakoukoli; při řeči v reálném tempu se
  to stalo (stroj stál za ARIALARM). Řetěz `INT 08h` tedy na Arii běžel
  až po potvrzení hardwarového přerušení. Emulátor proto vede výstup
  časovače 0 na INT2 (vektor `0Eh`, nikdo na B: ho nepoužívá,
  `tools/ints.py`); obsluha v ROM smaže západku tiku, pošle EOI a zavolá
  `INT 08h` softwarově; vlastní přerušení časovače 0 je vypnuté.
  Skutečné zapojení desky je neznámé **[předpoklad]**;

  **Neplatí od 8. 10. 2026:** takt není 4,77 MHz, ale **15 MHz**
  (FOSSIL, BKB a AAPI, doloženo v 6.4), a časovač 0 má periodu 1 ms.
  Odkud bral tik 18,2 Hz, je tím znovu otevřené. Emulátor má od téhož
  dne tik jako samostatný zdroj na desce (`AriaMachine::kTickNumerator`,
  1 193 182 / 65 536 Hz, protože BIOS počítá `1800B0h` tiků za den pro
  `INT 1Ah`), který dál vede na INT2; časovač 0 běží bez přerušení
  s maximem 3750 **[předpoklad zdroje tiku]**;
- **modelový bajt `FCh`** na `FFFF:000Eh` (čte ho `AAPI.EXE+46DEh`;
  hodnota Arie neznámá, 6.5);
- **past ESC zapnutá** (bit 15 relokačního registru): `VOICE.SYS+01CEh`
  instaluje obsluhu `INT 7`, což má smysl, jen když ji boot ROM zapnula;
  BIOS do té doby ESC přeskakuje stejně (posun o 2);
- **výchozí CMOS** `7Fh` = „R“ (nová podpora karet zapnutá), `7Eh` bez
  „C“ (bez externí klávesnice), 6.5;
  *(9. 10. 2026: „R“ je **stará** podpora fleš karet, tedy TRICKSS2, a
  výchozí 1 je doložená v ARIAMODE, viz 6.2 „Paměť DOSu“.)*
- **adaptér připojen, baterie nabitá** (`0700h` = `2Fh`, 6.9).

### 6.2 Mapa paměti a stránkování

1 MB NV RAM a 2 MB flash se nevejdou do 1MB adresního prostoru 80C188,
musí se stránkovat. Kandidáti: chip selecty PCB (`RAMDISK.EXE` zapisuje
port `00A0h` = UMCS při PCB na 0), porty `0480h`, `0580h`, `0702h`.
`0580h` je sdílený latch, do kterého zapisoval ovladač flash z boot ROM
(`FLASHDSK.SYS`) a který po startu spravuje `VOICE.SYS` přes `INT 69h`
(viz 6.1).

**Rozbor 6. 10. 2026 (7.05, rozbalené programy).**

*Disky.* Uživatel si pamatuje, že COMMAND.COM ROM-DOS se hlásil z disku
**A:**, což byl ROM disk v boot ROM s BIOSem a programy, které nemáme.
README.DOC: B: interní flash, C: karta, D: RAM disk, E: a dál Backpack.

*Okno D000h.* Celá flash (i karta) se čte jedním **oknem o 64 KB na
segmentu D000h**, které vybírají dva latche:

- `INT 69h` ve `VOICE.SYS` (`+0124h`–`+01ABh`) drží stínové slovo
  `[cs:116h]` (nižší bajt = latch `0580h`, vyšší = `0480h`) a zásobník
  tří uložených stavů. Funkce podle `CH`:
  - 0 uloží mapování (`AX=FFFFh`, když je zásobník plný), 1 ho obnoví
    a zapíše oba latche;
  - 2 namapuje stránku `DL`: `0580h = (DL << 2) | 2`, vrátí `DX = D000h`
    a `AX = BX` — tedy rovnou ukazatel `D000:BX`;
  - 3 namapuje stránku `DX` (10 bitů): bity 0–3 do `0580h` bitů 5–2 s
    režimem 3, bity 4–9 do `0480h` bitů 5–0, bit 7 latche `0480h` při
    `CL=0`; vrátí `DX = D000h`;
  - 4 okno vypne (`0580h = 0`).
- **Režim 2 = interní flash, režim 3 = karta PCMCIA.** Doklad: režim 2
  používal `FLASHDSK.SYS` z boot ROM (záplata ve `VOICE.SYS`) a AAPI
  (`AAPI.EXE+A807h`), režim 3 CardTrick (`DLFFSA.EXE+0E92h`), ovladač
  disku C:.
- **Stránka má 64 KB.** `AAPI.EXE+A807h` dostane 32bitovou adresu ve flash
  v `DX:BX`, `DX` jde jako stránka do `INT 69h CH=2`, `BX` je offset
  v okně; `AAPI.EXE+A818h` dělí kopii přes hranici 64 KB (`neg ax` =
  zbytek stránky) a mezi tím znovu mapuje. Šest bitů stránky v režimu 2
  = 4 MB adresního prostoru, 2 MB flash zabírá stránky 0–31.
- **Adresa ve flash = offset v obrazu disku B:.** AAPI hledá soubory
  přímo v FAT obrazu přes okno (`AAPI.EXE+A92Ch`: 256 položek adresáře
  po 32 B, porovná jméno, vrátí počáteční cluster z `+1Ah`). Emulátor
  tedy může okno v režimu 2 plnit přímo z `FlashImage`.
- `INT 62h` je obsluha v AAPI (`AAPI.EXE+A8D8h`): `AX=0200h` kopíruje
  z flash od základu `[4466h]` (dokumentované `AAPI_MEMCPY_SPEECH`),
  `AX=0201h` vrací adresu kopírovací rutiny. SPEAK.EXE tak čte fonémová
  data z flash oknem, ne přes DOS (`SPEAK.EXE+8B5Ch`).
- Smysl bitu 7 v `0480h` při režimu 3 (`CL=0`) zatím neznámý; u karty
  PCMCIA by to mohl být výběr atributové/společné paměti (REG#)
  **[předpoklad]**.

*RAM disk D: pod boot ROM.* `RAMDISK.EXE` je **Datalight VDISK 5.0**
(rev. 1.56-B) v režimu „static VDISK“ se základem **B000h**
(`RAMDISK.EXE+01BFh`) a výchozí velikostí **319 KB**, sektor 512 B,
128 položek adresáře (DGROUP na odstavci C0h, `DS:[0054h]`–`[0058h]`;
CONFIG.SYS žádné parametry nedává). 319 KB = 4FC00h, takže disk leží na
**B0000h–FFBFFh**. Při každém přístupu (`RAMDISK.EXE+0372h`–`+03A9h`):
uloží UMCS (`in ax,0A0h`) a mapování (`INT 69h CH=0`), zakáže přerušení,
**nastaví UMCS = FFF8h** (horní chip select jen na 1 KB na FFC00h–FFFFFh)
a **vypne okno** (`out 0580h,0`), zkopíruje, a vše vrátí. RAM pod oblastí
boot ROM je tedy vidět jen se zmenšeným UCS; okno D000h leží uvnitř a
musí být vypnuté. Horní 1 KB (resetovací vektor) zůstává ROM vždy.

Otevřené k 6.2: normální hodnota UMCS z boot ROM (velikost oblasti ROM),
co je na A0000h–AFFFFh a kolik RAM vidí DOS. Stav a napájení slotu karty
(`0480h`, `0702h`) jsou v 6.8; první verze tohoto odstavce měla hodnoty
`0702h` obráceně.

**Paměť DOSu a textový editor, 9. 10. 2026.** Uživatel hlásil, že F1
dá „plná paměť“ a anglicky „please disable old flash card support in
mode“, a přepnutí v Mode nepomohlo. Zjištěno:

- Obě hlášky říká **MAINMENU**, ne editor: `MAINMENU.EXE+0351h`–`+0372h`
  spustí program, a když DOS vrátí chybu 8 (málo paměti), řekne zprávu
  16h `IDS_OUT_OF_MEMORY` a 276h. Formát MESSAGES.DAT: slovo 0, slovo
  počet (D9Dh), tabulka 32bitových offsetů, na offsetu bajt délky a text.
- **AWP.EXE potřebuje 248 624 B** (obraz + minimální alokace z hlavičky
  MZ + PSP); volných bylo **172 KB** (640 KB, jádro FreeDOS v DOS paměti).
- **„Podpora staré fleš karty“ je TRICKSS2** (42 KB rezidentně): položka
  Mode se zprávou 274h má obsluhu `ARIAMODE.EXE+1361h`, která do CMOS
  `057Fh` zapíše „R“ při hodnotě 1 (tabulka položek `ARIAMODE.EXE`
  soubor `+4856h`, 28h bajtů na položku, hodnota na `+16h`; výchozí 1,
  u externí klávesnice 0). Výchozí „R“ v emulátoru tedy sedí s Arií.
  **Oprava 6.5:** „R“ neznamená novou, ale **starou** podporu karet.
  Změna v Mode se projeví až při dalším startu (TRICKSS2 čte CMOS v
  AUTOEXEC), a dokud se CMOS neukládala, každé spuštění ji zapomnělo.
- Uživatel potvrdil, že na Arii editor s výchozím nastavením (TRICKSS2
  zavedený) **šel spustit**. ROM-DOS běžel z ROM; FreeDOS drží rezidentní
  kód (skupina HMA_TEXT, 40,6 KB) v bloku „SC“ v paměti DOSu, když nemá
  HMA (`config.c` configDone), a 80C188 HMA nemá.

**Řešení:** (1) DOS má **704 KB** (`INT 12h`, BDA): 640 + 64 KB na
A0000h + 319 KB RAM disku od B0000h + 1 KB boot ROM je přesně 1 MB RAM;
na B: nic velikost neudává **[předpoklad]**. (2) **Jádro v ROM:** UMCS od
**F0000h** (`AriaMachine::kRomArea`, oblast ROM až po FFBFFh, zapisovatelná
jako stínová RAM, mimo snímek paměti); RAM disk pod ní je dostupný, protože
RAMDISK.EXE na každý přístup zmenší UMCS na FFF8h se zakázaným
přerušením. Začátek F0000h je **[předpoklad]**. UMCS jinak píše jen
obsluha IRQ 13 koprocesoru z knihovny Borlandu v ARIACLK a CALC
(`out 0A0h,20h`, EOI druhého PIC na PC), která na Arii nikdy neběží.
BIOS při `INT 12h` (jádro je už rozbalené, `main.c` init_oem) najde v
paměti právě jeden výskyt configDone (alokace `allocmem`, MCB „SC“,
volání MoveKernel; v ke2046 `026B:ADFD`) a ověří prolog MoveKernel s
proměnnou CurrentKernelSegment (`DS:0592h`). Volání `allocmem` nahradí
pastí `0Fh F0h`; ta zkopíruje kód do `F001:0000` a vrátí ten segment
(MCB „SC“ pak padne do oblasti ROM, ne na D:). MoveKernel sám kopíruje
jen směrem dolů (`inithma.c`), proto kopie v pasti; pak přesměruje
dálkové skoky. **Výsledek:** 278 KB volných s TRICKSS2, F1 spustí AWP.
`boot_test` (7.05 i 6.87) to ověřuje: žádný blok „SC“, největší volný
blok ≥ potřebě AWP z hlavičky, po F1 blok „AWP“ v řetězu MCB a žádné
„please disable“. Při jiném jádře se nic nepatchuje a test selže.

### 6.3 Rozhraní DSP

`SPEAK.EXE` programuje DMA kanál 0 (`C0h`–`CAh`) a zapisuje porty `0401h`
a `0402h`. Neznámé: jak se do ADSP-2105 nahraje `LPC.DSP` (2105 nemá
host port, bootuje z byte-wide boot paměti), jak se předávají rámce a kam
vede zvukový výstup (sériový port DSP → DAC?).

**Z běhu, 7. 10. 2026** (rezidentní SPEAK na segmentu `4732h`, data
`5237h`, CC0705):

- SPEAK přebírá **`INT 0Ah` = přerušení DMA kanálu 0** (typ 0Ah na
  80186), obsluha `4732:0B62`. Je to pumpa řeči: z kruhové fronty rámců
  (8 KB na `DS:1B72h`, ukazatele `1B6Eh`/`1B70h`) pošle další rámec,
  zvláštní kódy rámců `50h`, `FAh`, `FBh`, `FCh` mají vlastní větve.
  Příznaky `[F2h]`, `[F4h]`, `[F6h]` řídí stav.
- Čekací smyčka `4732:62A0` čeká, až čítač času (`call 0774h`) dosáhne
  zadané hodnoty, nebo až se nastaví `[F0h]`/`[F4h]`; mezitím čte
  řízení přerušení DMA0 (`in 34h`) a při bitu 3 (maska) ho přepíše na 7.
- DMA kanál 0 jde na port `0401h` synchronizovaný požadavkem DRQ0, který
  musí dávat DSP. Bez DSP se nic nepohne. Při startu SPEAK zapíše na
  `0401h` 886 bajtů (nejspíš zavedení `LPC.DSP`) a na `0402h` jeden.

Co z toho je pro emulaci: dokud DSP není, stačí na straně DSP **jímka**,
která drží DRQ0, dokud je DMA0 zapnuté, a bajty si zapisuje; řeč bude
tichá, ale SPEAK neuvázne. Dál je třeba zjistit význam `0402h` a zda
SPEAK něco z DSP čte zpět.

**Rozhraní hostitel–DSP, 7. 10. 2026** (rozbor `SPEAK.EXE` 7.05, který
není PKLITE, a běh s jímkou):

- **`0401h` je řídicí latch.** SPEAK drží stínovou kopii na `DS:561Ch`,
  bity nastavuje a maže pomocníky `+897Eh` (OR) a `+898Dh` (AND) a zapíše
  vždy `(kopie XOR 7) OR 8` (`+896Ah`). Za běhu startu 3 580 zápisů.
  Sekvence `+89BEh` (nastav 5, smaž 1, smaž 2, prodleva, nastav 2,
  prodleva, nastav 1) vypadá na řízení resetu a výběru, význam bitů
  zatím neznámý.
- **`0402h` je datový port DSP a jediná cesta dat.** Žádný program na B:
  z `0401h` ani `0402h` nečte; DSP hostiteli odpovídá jen dvěma linkami.
- **Zavedení programu DSP (`+09EAh`):** `0401h` ← 0, `0402h` ← bajt
  (`[53BAh]` nebo `[53BBh]`), prodleva, `0401h` ← 8, pak DMA0 z
  `DS:4186h` nebo `DS:47B6h` (délka `[53B6h]`/`[53B8h]`) na port `0402h`,
  řídicí slovo `76A6h`: bajty, paměť → I/O, synchronizace cílem (DRQ0),
  stop na konci, bez přerušení. **Ověřeno během:** proud do `0402h`
  začíná bajt po bajtu celým `SYSTEM\LPC.DSP` (2 834 B). ADSP-2105 nemá
  hostitelský port a zavádí se z bajtové boot paměti; na Arii ji
  zjevně hraje 80188, který program DSP vysype DMA kanálem do `0402h`.
- **Příkazy:** před každým bajtem SPEAK zapíše do řízení INT3 (`3Eh`)
  `1Fh` (maskováno, úrovňově) a čte žádosti (`2Eh`), dokud bit 7 (INT3)
  nehlásí připravenost DSP (`+0A78h`, `+67A1h`, `+68C8h`). INT3 je tedy
  linka „DSP připraven na bajt“.
- **Rámce řeči:** obsluha `INT 0Ah` (`+0B62h` v souboru, rezidentně
  `4732:0B62`) pošle DMA0 další rámec z kruhové fronty do `0402h`,
  řídicí slovo `77A6h` (jako zavedení, navíc přerušení na konci).
- **Obsah proudu za start 7.05 (32 261 B):** `LPC.DSP` (0–2 833), pak
  3 582 B rámců řeči, které začínají slovem „Sync“ (asi 19 B na rámec,
  kolem 188 rámců), pak od bajtu 6 416 celý `PLAY.DSP` (985 B), krátká
  hlavička se slovem „Play“ a za ní **`SYSTEM\SOUNDS\HARPUP.04`**, harfa
  ze startu, kterou popisuje prohlídka Robotronu z roku 1997 (2).

**Jímka v emulátoru (`AriaMachine`, 7. 10. 2026):** DRQ0 a INT3 drží
trvale aktivní, bajty z `0402h` ukládá (`DspBytes()`, `boot_test -dsp`),
zápisy do `0401h` počítá. Řeč je tichá; s jímkou start doběhne až do
MAINMENU (6.10). `boot_test` ověřuje, že DSP dostal nejdřív celý
`LPC.DSP` a později `HARPUP.04`, oboje čtené z načteného obrazu. Pro
skutečnou řeč (fáze 4) zbývá: jádro ADSP-2105, význam bitů `0401h`,
formát zaváděcího proudu a rámců „Sync“/„Play“ a cesta zvuku z DSP.

**Zdroje k ADSP-2105 (7. 10. 2026):**

- *ADSP-2100 Family User's Manual*, 3. vydání, září 1995 (Analog
  Devices; kopie na Bitsavers,
  `components/analogDevices/dsp/ADSP-2100_Users_Manual_3ed_Sep95.pdf`,
  492 stran). Mimo repozitář, jako techman. Kapitola 10.4 popisuje
  zavádění, kapitoly o instrukcích, SPORT a časovači zbývá přečíst.
- **Jádro MAME** `src/devices/cpu/adsp2100/` na commitu `2aa75c50`
  (tentýž jako jádro 80188): `adsp2100.cpp`, `adsp2100.h`, `2100ops.hxx`,
  `2100dasm.cpp`/`.h`, licence **BSD-3-Clause** (Aaron Giles), model
  `adsp2105_device` s vlastními přerušeními (IRQ0–2, SPORT1 příjem a
  vysílání, časovač). Jeho `load_boot_data` ale čte rozložení desek
  automatů (4 bajty na slovo), ne bajtový proud Arie; zavádění si
  emulátor udělá sám podle manuálu.

**Formát zaváděcího proudu, ověřeno 7. 10. 2026** (manuál 10.4.1 a
10.4.4, který výslovně zmiňuje případ, kdy data místo EPROM dává
latch): DSP nejdřív přečte bajt délky stránky, stránka má
(délka + 1) × 8 slov; pak načítá slova **od posledního k nultému**,
každé jako tři bajty v pořadí **horní, dolní, prostřední**. Stránka má
tedy 1 + 3 × slova bajtů a program začíná na adrese 0, až dorazí slovo 0.

- `PLAY.DSP`: délka `28h` → 328 slov → 985 B, přesně velikost souboru.
- `LPC.DSP`: délka `75h` → 944 slov → 2 833 B; soubor má o jeden nulový
  bajt víc a i ten SPEAK pošle (v proudu bajt 2 833); nejspíš výplň.
- Dekódovaná slova jsou smysluplný kód ADSP-2100: `PLAY.DSP` slovo 0 =
  `1801CFh` = „JUMP 001Ch“ (vektor resetu přeskakuje tabulku
  vektorů), slovo 4 = `180C6Fh` = „JUMP 00C6h“ (další vektor po čtyřech
  slovech); `LPC.DSP` začíná `340004h`, `340015h`, `340036h` (načtení
  registrů), `1801CFh` (JUMP 001Ch), `1C05AFh` (CALL), `0A001Fh` (návrat).

**Jádro v emulátoru (7. 10. 2026):** `src/adsp2100/` je jádro MAME
(kopie `2100ops.hxx` a `adsp2100.cpp`, framework vypnutý bloky `#if 0
// ARIA:`, deklarace přepsané v `adsp2100.h`), `src/adsp2105.*` k němu
dodává paměti čipu (program 1K slov `0000h`–`03FFh`, data 512 slov
`3800h`–`39FFh`, řídicí registry `3C00h`–`3FFFh` zatím jen uložené)
a zavádění stránky podle 10.4.4. `dsp_selftest` (bez firmwaru) ověřuje
pořadí bajtů zaváděcí stránky a krátký program (5 + 3 = 8 do datové
paměti). Historie MAME: poslední změny chování jádra jsou z roku 2023,
změny z roku 2026 jen úklid frameworku a disasembler; přechod na novější
MAME je proto levný.

**První běh `LPC.DSP` (pokus mimo strom, 7. 10. 2026):** zavedení spotřebuje
přesně 2 833 bajtů; během prvních 42 instrukcí program zapíše tři řídicí
registry: `3FF2h` (řízení SPORT1) ← `4C8Fh`, `3FF1h` (SCLKDIV SPORT1) ←
`0004h`, `3FEFh` (autobuffer SPORT1) ← `0E82h`; pošle SPORT1 jedno slovo
`0000h` a pak čeká v IDLE na `0043h`–`0044h`. Podle manuálu (kapitola 5)
bit 14 = ISCLK, takže **sériové hodiny generuje DSP**, a to CLKOUT /
(2 × (4 + 1)) = CLKOUT / 10; spodní čtyři bity `Fh` = **16bitová slova**.
Ostatní bity (rámcová synchronizace, autobuffer v `3FEFh`) a odkud DSP
bere bajty z `0402h` (přerušení? vnější datová paměť?) jsou další krok.

**Kód `LPC.DSP` (7. 10. 2026, disasembler MAME `bin/dsp_disasm.exe`,
výpis jen mimo strom):** vektory 2105 jsou po čtyřech slovech od `0004h`
(MAME `adsp2101_device::generate_irq`):

- `0004h` IRQ2: `CALL 005Ah; RTI`. Rutina čte **`AX0 = DM($3000)`**
  (`005Fh`, `0082h`) a stavovým automatem hledá bajty `53h 79h 6Eh 63h`
  = **„Sync“** (`0062h`–`0077h`), pak čte rámec. Bajty z portu `0402h`
  tedy DSP vidí na **vnější datové adrese `3000h`** a IRQ2 je „bajt
  čeká“. Jiný vnější přístup program nemá a nic nezapisuje ven.
- `0010h` SPORT1 vysílání: další vzorek. Když fronta dojde, skok na
  `010Eh` smaže bit 11 systémového řídicího registru `3FFFh` (podle
  manuálu povolení SPORT1), takže DSP v tichu vzorky neposílá.
- Nastavení (`003Bh`–`0043h`): SPORT1 jako výše, `ICNTL = 14h` (IRQ2 na
  hranu, vnořování), `IMASK = 24h` (povolené jen IRQ2 a vysílání SPORT1).
- `FL1`/`FL2` „zásahy“ na `0313h`–`0339h` jsou data (tabulky od `0300h`),
  ne kód: program **žádný vlastní výstupní pin nepoužívá**.

**Výklad: jednobajtová schránka (latch) na desce.** 80188 zapíše bajt do
`0402h` → latch je plný a dává DSP hranu na IRQ2; DSP přečte `DM($3000)`
→ latch je prázdný a hlásí 80188 „další“ (DRQ0 pro DMA, INT3 pro
jednotlivé příkazové bajty, 6.3 výše). Při zavádění tentýž latch slouží
jako zaváděcí paměť: zaváděcí logika DSP čte bajt po bajtu a adresa je
jedno. Zda DRQ0 a INT3 jsou jeden signál, nebo dva, se z kódu nepozná;
model je oba drží „připraveno“, když je latch prázdný. **[odvozeno, ne
doloženo schématem]**

**DSP ve stroji (7. 10. 2026).** Jímka výše (DRQ0 a INT3 trvale aktivní)
už neplatí: `AriaMachine` má skutečný ADSP-2105 a schránku. Zápis do
`0402h` naplní schránku; během zavádění ji zaváděcí logika hned
vyprázdní, za běhu dá DSP hranu na IRQ2 a DSP ji vyprázdní čtením
`DM($3000)`. Prázdná schránka dává 80188 DRQ0 i INT3. `0401h` bit 3
z 0 na 1 resetuje DSP a spustí zavádění stránky 0 ze schránky, 0 drží
reset. CPU a DSP se střídají po 64 taktech CPU. `Adsp2105` přidal podle
manuálu: SPORT1 vysílání s autobufferem (5.11; TX1 → posuvný registr,
další slovo z `DM(I,M)` s kruhovým adresováním MAME, přerušení při
přetočení), IDLE, které opravdu čeká na přerušení (MAME jen ukončí
úsek; malá změna `ARIA:` v jádře si pamatuje zbylé takty), a vnitřní
přerušení SPORT1, které se při převzetí maže. `dsp_selftest` to ověřuje
programem s autobufferem čtyř slov.

Předpoklady, které nic nedokládá:

- **Takt DSP 10 MHz** (`AriaMachine::kDspHz`), běžný stupeň 2105.
- **ICNTL = 0 po resetu.** Manuál (3.4.2.1): „undefined after a
  processor reset“, MAME ho resetem nemění. `PLAY.DSP` ICNTL nenastavuje,
  a přitom obě jeho obsluhy přerušení používají sekundární banku
  registrů (`ENA SEC_REG` na `00C6h` a `011Bh`), což je bezpečné jen bez
  vnořování. S ICNTL = 14h, které zůstalo po `LPC.DSP`, se IRQ2 vnořilo
  do obsluhy SPORT1 a přepsalo AR mezi `0120h` a `0123h`: systémový
  registr dostal `0059h` (SPORT1 přepnutý na piny) a vše se zaseklo.
  S nulou start doběhne.
- **Délka slova SPORT1** = (SLEN + 1) × 2 × (SCLKDIV + 1) taktů, slova
  za sebou. Rámcová synchronizace je ale vnější (4C8Fh: ITFS = 0), takže
  skutečná vzorkovací frekvence je dána deskou.

**Výsledek startu 7.05:** pět zavedení (`LPC.DSP`, `PLAY.DSP`…), celý
proud 32 261 B projde schránkou, stroj dojde do MAINMENU a čeká na
klávesu v `INT 16h` BIOSu. Ze SPORT1 vyjde 197 833 vzorků: 188 361 při
SCLKDIV 4 (řeč z `LPC.DSP`, z asi 188 rámců „Sync“) a 9 472 při
SCLKDIV 19 (harfa; `HARPUP.04` má 9 431 B, tedy vzorek na bajt). Totéž
na 6.87 a 6.99; anglická 7.04 má `LPC.DSP` se **SCLKDIV 6** (`004Ch`),
mluví tedy 5/7 frekvencí české řeči. `LPC.DSP` potřebuje v průměru asi
210 instrukcí na vzorek (včetně ticha), při 10 MHz tedy nejvýš asi
47 000 vzorků za sekundu.

**Vzorkovací frekvence zůstává otevřená.** Poměry jsou jisté (trvání
vzorku ∝ SCLKDIV + 1, protože deska odvozuje rámcovou synchronizaci ze
sériových hodin; tabulka `PLAY.DSP` na `0142h` = 19, 19, 13, 9, 6),
absolutní hodnota ne. `boot_test -wav CESTA FREKVENCE` vykreslí výstup
do WAV 44,1 kHz s danou frekvencí pro SCLKDIV 4; rozhodne poslech
(uživatel PeterTalk zná).

**Poslech, 7. 10. 2026:** ze tří variant (16 000, 22 050 a 32 000 Hz pro
SCLKDIV 4) zní nejvěrněji **32 000 Hz**, „možná ještě o kousek výš“;
harfa při ní (8 kHz) „zní docela věrně“. Pracovní hodnota je tedy
32 kHz pro českou řeč. Hypotéza, která k tomu sedí: deska dává
rámcovou synchronizaci každých 32 period sériových hodin (16bitový
vzorek a 16 volných bitů), takže frekvence = takt DSP / (2 × (SCLKDIV
+ 1) × 32); při 10 MHz to je 31,25 kHz pro řeč a 7,8 kHz pro harfu,
při o něco rychlejším krystalu o kousek víc. **[hypotéza]**

**Zvuk v reálném čase (7. 10. 2026).** Emulátor tu hypotézu přijal:
SPORT1 posílá slovo za 32 period sériových hodin a takt DSP je
**10,24 MHz** (`AriaMachine::kDspHz`), takže česká řeč má přesně 32 kHz
a harfa 8 kHz. Dřívější model (slova hned za sebou, 62,5 kHz) už
neplatí. `DacRenderer` převádí slova se značkou času v taktech DSP na
44,1 kHz (lineárně mezi blízkými slovy, ticho, když SPORT mlčí) a
`EmulatorThread` (vzor Eureka) pouští stroj v reálném čase do
`AudioPlayer` (z Eureky beze změny) s latencí kolem 30 ms. Při tomto
tempu se ukázalo, že tik musí jít přes INT2 (6.1).

**První poslech aplikace, 7. 10. 2026 (uživatel):** start trvá dlouho
(„Aria tak dlouho nestartovala, ale okamžitě to nebylo“), po harfě je
před řečí **tvrdé lupnutí**, v řeči jen drobné špičky.

- **Lupnutí:** `PLAY.DSP` ukončí harfu osmi vzorky `2007` (plný rozsah:
  µ-law maximum 8031 posunuté o 2 bity doprava, dekodér `009Bh`), pak
  ticho a přepnutí na `LPC.DSP`. Zvuky chodí v blocích „**Play**“:
  „Play“, kontrolní součet, počet bajtů (`80h` = 128) a formát (`04h`),
  pak data (parser `00E6h`–`0118h`, stavy `[3908h]`); program bajty
  ukládá surové do bufferů v programové paměti `180h`–`27Fh`, za koncem
  vlastního obrazu (328 slov), a dekóduje je až při přehrávání. Než je
  naplní, leží v nich zbytky kódu `LPC.DSP` (RAM přežije reset i na
  skutečném čipu); na konci harfy jich pár přehraje, než SPEAK DSP
  resetuje. Zda Aria lupala také, závisí na časování (jak brzy SPEAK
  resetuje), a takty CPU i DSP jsou předpoklady; otevřené.
- **Rozsah vzorků:** všechny vzorky startu jsou v −1 498 až +2 007, tedy
  ve 12 bitech, a `PLAY.DSP` posouvá o 4 bity doprava (`005Ah`): převodník
  je nejspíš **12bitový** **[předpoklad]**. Emulátor posílá hodnoty tak,
  jak jsou, takže hraje o 24 dB tišeji, než by mohl.
- **Doba startu** (`boot_test -v`, sekundy emulovaného času od zapnutí):
  jádro FreeDOS 1,8; VOICE.SYS 3,8; disk D: 4,7; Backpack 4,9–5,1;
  `aapi` 7,2 → hotovo 9,4; SPEAK 10,7; BKB 10,9; ARIALARM 15,0; MAINMENU
  16,9; **první zvuk 18,9**. Žádné čekání na časový limit, čas se
  rozkládá na zavádění a rozbalování programů: start je vázán na takt CPU
  (předpoklad 4,77 MHz kvůli tiku, 6.1). Ušetřené 2 s: BIOS v kopii jádra
  v paměti nuluje `SkipConfigSeconds` (konfigurační blok „CONFIG“ na
  offsetu 2, pole na offsetu 12, `kernel.asm`), takže FreeDOS nečeká na
  F5/F8, na které stejně nemá kdo odpovědět.

  **Od 8. 10. 2026 při 15 MHz** (6.4): jádro 0,6; VOICE.SYS 1,2; disk D:
  1,9; `aapi` 3,0 → hotovo 3,7; BKB 4,1; ARIALARM 5,5; MAINMENU 6,1;
  **první zvuk 6,7**. Start je tedy vázán na takt, jak se čekalo.
  Uživatel 8. 10. 2026: prodleva do harfy odpovídá jeho vzpomínce na
  Arii. Zároveň **zmizel ráz (lupnutí) po harfě** a při odchodu
  z aplikací, který byl slyšet při 4,77 MHz (`ar-5hf`). Příčina
  nezměřena; nejspíš pomalé CPU nestíhalo krmit DSP.

  Další neznámé: význam zápisů do vnější
programové paměti `2000h` (PX = 1 v `LPC.DSP` `001Eh`, `0115h`; PX = 3
v `PLAY.DSP` `0073h`), nejspíš latch na desce (výstup? frekvence?).

Aria tedy zavádí DSP standardní zaváděcí stránkou 0 a 80188 hraje roli
EPROM. Zápis `0401h` ← 0 a pak 8 kolem zavádění (výše) je nejspíš reset
DSP; zda bit 3 je reset, nebo vstup MMAP či BMODE, se ukáže z kódu, až
poběží jádro.

### 6.4 Klávesnice

`BKB.COM` čte `0700h` (5×), `0480h`, `050Dh`, `0581h`, `0584h`; instaluje
`INT 13h` a `INT 16h`. Rozložení braillských kláves zatím neznámé.

**6. 10. 2026 (BKB 7.05, ARIAKBD.PAS, ARIA-API.H, API.DOC):**

- **Matice kláves** se čte třemi porty, jeden adresní bit na řádek:
  `0581h`, `0582h`, `0584h` (`BKB.COM+02EAh`–`+02FEh` porovnává se
  stavem, `+036Ch` maskuje `0584h` `0Fh` a `0582h` `FFh`). Tři bajty
  odpovídají `KbdGetPowerOnState` z API: nízký bajt braillské klávesy,
  střední funkční, vyšší kurzorové. Který port je který řádek, zatím
  nerozhodnuto. Zápis na `0580h` je přitom latch okna (6.2) — čtení a
  zápis jsou na desce dekódované zvlášť.
- ~~**`0700h` stav:** bit 4 je zvláštní klávesa … nejspíš vypínač~~ —
  **neplatí**, viz níže: `0700h` je stav baterie a nabíječe.
- **`0700h` = baterie a nabíjení** (doloženo hláškami): BKB načte
  `LoadStringEx(0C00h, …, 1Dh)` (AAPI pořadí 66h, `BKB.COM+186Ah`–`+18A3h`)
  do tabulky ukazatelů od `[289h]`; v ARIAMSG.H jsou to `IDS_KBD_CHARGING`
  0C00h, `FULLY_CHARGED`, `100`, `75`, `50`, `25`, `FLAT`, `LOW_BATTERY`
  0C07h, … `RTCFLAT` 0C10h, `TRICKLE_CHARGING` 0C1Ah, `PCCARD_LOW/FLAT`
  0C1Bh/0C1Ch. Rutina hlášení baterie `BKB.COM+15B5h`:
  - bit 4 = 1 → **běh z baterie**; bity 0–3 stav nabití, nejnižší
    nastavený = 100 / 75 / 50 / 25 %, žádný = vybitá;
  - bit 4 = 0 → **adaptér připojen**: bit 5 = 0 nabíjí se; bit 5 = 1
    a bity 0–3 všechny = nabito, jinak udržovací nabíjení.
  - `+1A5Ch` (časovač): z baterie a vybitá 32 tiků → zruší řeč, událost
    11h, hláška `IDS_LOW_BATTERY` — vypnutí při vybité baterii.
  - `+160Bh`: registr D RTC bit 7 = 0 → `IDS_KBD_RTCFLAT`, vybitá
    baterie hodin. `+1622h`: BVD karty → `IDS_KBD_PCCARD_LOW/FLAT`.
- **Vzpomínky uživatele (6. 10. 2026):** kurzorové klávesy tvoří kříž
  jako ciferník — nahoru na 12, vpravo na 3, dolů na 6, vlevo na 9 — a
  uprostřed je malá kulatá klávesa **Shift** s vystouplým bodem; stiskem
  Shift se ztlumí řeč. Sedí to s kódy kláves: kurzorový bajt má čtyři bity
  šipek a `K_SHIFT = 10h`. BKB maskuje jeden řádek matice (`0584h`) `0Fh`,
  tedy jen šipky, takže **`0584h` je nejspíš řádek kurzorového kříže**,
  `0581h`/`0582h` braillské a funkční klávesy **[předpoklad]**.
- **Kódy kláves AAPI** (ARIA-API.H, ARIAKBD.PAS): funkční `1C0h + n`
  (F1–F12), kurzorový blok je **akord** `180h + maska` (nahoru 1, dolů 2,
  vlevo 4, vpravo 8; Home = nahoru+vlevo, PgDn = dolů+vpravo, Ins, Del…),
  Shift `+10h`, Alt `+20h` = **mezerník na BKB**. Akordy z API.DOC:
  Mode = mezera+bod 1, Kde jsem = mezera+bod 4, Čas = mezera+body 2345,
  Datum = mezera+body 145, Data ROM = mezera+body 1235, Vložit/přepsat =
  mezera+body 24, Baterie i Kontrolní součet = mezera+body 12;
  `K_SLEEPY = 424Eh` dostane program jako klávesu před vypnutím.
- **Služby BKB:** `INT 64h` = vlož klávesu `CX` do vlastní fronty BKB
  (`+0DA2h`; API.DOC: „called by the keyboard driver to add a keystroke“;
  AAPI ho zachytává kvůli Up pro virtuální obrazovku); `INT 6Ah` = echo
  znaku z vnější klávesnice (`+05E8h`, třídy malé/velké/číslice/jiné,
  volá ATKBD); **`INT 16h` celé nahrazené** (`+0DC5h`) — standardní
  funkce 00h–03h, 05h, 10h–12h a rozšíření Arie **21h–31h, FEh, FFh**
  (22h = `KB_GETSTAT`, 2Eh = povolit/zakázat).
- AAPI z Arie volá rozšíření 21h, 22h, 24h–27h, 29h, 2Ah, 2Eh, 30h;
  aplikace přímo jen 2Eh (ARIALARM, MAINMENU, ZM), 2Fh (ARIAMODE) a
  30h (MAINMENU). Významy 21h–31h zatím neznámé.

**Rozložení matice a snímání, 7. 10. 2026 (dekódování v `BKB.COM`):**

- **Stisknutá klávesa čte 1.** Snímání `+02E5h` porovná porty se
  snímkem (`[13Ch]`–`[13Eh]`), při změně je čte znovu (`+036Ch`): `0584h`
  s maskou `0Fh`, pak `0582h` a `0581h`; nově stisknuté bity (`nové AND
  NOT staré`, `+038Eh`) nastaví příznak, který přes `INT 61h` AX=0101h
  ztiší řeč. Akord se sbírá, dokud se nepustí, a vydá se jednou.
- **`0581h`:** bity 0–5 = **body 1–6**, bit 7 = **mezerník** (z kódu se
  stane modifikátor 20h, „Alt“ z API), bit 6 se nepoužívá. Pořadí bodů
  (bit 0 = bod 1) sedí s akordy, které rutina `+048Ch` rozlišuje (mezera
  + 01h, 02h, 04h, 08h, 20h, 0Bh, 25h, 31h, 38h…) a s akordy z API.DOC.

  **Neplatí od 8. 10. 2026:** body 1–3 jsou obráceně, bity 0–5 jsou
  **body 3, 2, 1, 4, 5, 6** (pořadí kláves zleva doprava). Uživatel to
  slyšel při zkoušce (`F` dávalo bod 3, `S` bod 1) a dokládá to celý
  seznam akordů s mezerou (`+052Eh`–`+0578h`): `1Bh` → `CAh` a `1Ch` →
  `CBh` jsou čas (body 2345) a datum (body 145) z API.DOC jen v tomto
  pořadí; v původním by to byly akordy, které nikdo nepopisuje, a čas a
  datum (`1Eh`, `19h`) by v seznamu chyběly. Stejně `04h` → `C8h` je
  Mode (mezera + bod 1) a `06h` → `D8h` baterie (mezera + body 12).
  Akordy jako `0Bh` z odstavce výše byly souměrné, proto pořadí
  nerozlišily.
- **`0582h`:** bity 0–7 = **F1–F8** (`+0459h`: kód C0h + číslo bitu, v API
  1C0h + n).
- **`0584h`:** bity 0–3 = **kurzorový kříž** nahoru 1, dolů 2, vlevo 4,
  vpravo 8 (kód 80h | maska = API 180h + maska), bit 6 = **Shift**
  uprostřed kříže (modifikátor 10h, `+0467h`). Předpoklad z 6. 10. (řádek
  `0584h` = kříž) tím je doložen.
- **Snímání běží v přerušení časovače 2.** BKB volá `+02E5h` jen ze svého
  háku na `INT 13h` (`+1A40h`, s kontrolou baterie `0700h`). Na 80186 je
  typ 13h přerušení časovače 2, takže „INT 13h“ je na Arii periodické
  hardwarové přerušení, ne disk; B: obsluhoval `FLASHDSK.SYS` oknem. Bez
  běžícího časovače 2 BKB klávesy vůbec nevidí (ověřeno: MAINMENU stál v
  `INT 16h` AH=22h a na akord nereagoval).

**V emulátoru (7. 10. 2026):** BIOS pouští časovač 2 s přerušením a
maximem `C925h`, jaké nastavuje hák AAPI (`AAPI.EXE+9F20`), tedy 23,2
snímání za sekundu při 1,19318 MHz. Vektor 13h zároveň potřebuje disk
FreeDOSu: obsluha `INT 13h` v BIOSu pozná přerušení časovače podle
in-service registru PCB (bit 0) a jen ho potvrdí, jinak obslouží disk.
Hostitelské klávesy podle Eureky: `F D S` = body 1–3, `J K L` = body
4–6, mezerník, `F1`–`F8`, šipky = kříž, `Shift` = Shift, `Home`/`End`/
`PgUp`/`PgDn` = dvojice šipek (API: Home = nahoru + vlevo, PgDn = dolů +
vpravo), `Alt+F4` zavírá okno. Každá klávesa zůstane stisknutá aspoň
150 ms emulovaného času, aby ji periodické snímání nepřehlédlo; autorepeat
Windows se zahazuje. `boot_test` po startu stiskne mezeru + bod 4 („kde
jsem“) a MAINMENU odpoví řečí na 7.05, 7.04, 6.99 i 6.87.

**Zkouška uživatele a oprava, 8. 10. 2026:** krátce, ale vědomě
zahraný akord nevydal nic; klávesy bylo nutné držet asi 200 ms (stejně
je to v emulátoru Eureky). Příčina: BKB bere akord, až se matice po
poslední změně nezmění po **tři snímání** (`+0350h` nastaví `[149h]` =
3, `+031Ah` odpočítává, `+03AFh` akord převezme; při puštění se uložený
stav přepíše nulou, takže se akord bere za držení, ne při puštění).
Rychlý akord má klávesy dole jednu po druhé a 150 ms od stisku každé
zvlášť nestačilo, aby byly dole **všechny současně** dost dlouho.
**Neplatí** proto „aspoň 150 ms na klávesu“: emulátor nepustí žádnou
klávesu, dokud od posledního stisku kterékoli neuběhne pět period
časovače 2 (`C925h` × 4 takty CPU; jedno snímání na zjištění změny, tři
na odpočet, jedno rezerva). Při dnešním taktu 4,77 MHz je to 216 ms,
což je pořád pomalé, protože snímání je třikrát pomalejší než na Arii:

- **Arie snímala asi 72× za sekundu, ne 23×.** Tabulka zpoždění
  autorepeatu pro `INT 16h` AH=03h (`BKB.COM+1012h`) má pro PC hodnoty
  250, 500, 750, 1000 ms **18, 36, 54, 72** snímání a tabulka rychlostí
  (`+1016h`, 32 položek) 3 až 34 snímání pro 30 až 2 znaky za sekundu.
- **Takt CPU je podle tří nezávislých míst 15 MHz** (časovače 3,75 MHz):
  dělitele rychlostí FOSSILu (`FOSSIL.COM+0793h`: 2131, 1562, 781, 390,
  195, 98, 49, 24, 12, 8, 6, 4, 3, 2) krát rychlost krát 16 dávají
  3,75 MHz pro 110 až 115 200 Bd (krystal 14,7456 MHz by dal 2095, 1536,
  768…); `C925h` při 3,75 MHz je 72,8 snímání za sekundu, což sedí na
  tabulku autorepeatu; zpožďovací smyčka `AAPI.EXE+10B35` čeká na
  průchod čítače časovače 0 hodnotou `753h` = 1875, tedy polovinou
  periody 3750, a ta je při 3,75 MHz přesně **1 ms** (smyčka počítá
  periody podle argumentu). Předpoklad 4,77 MHz z 6.1 tím padá; změna
  taktu je další krok, protože tik 18,2 Hz pak nemůže dělat 16bitový
  časovač 0 (6.1).

  **Provedeno týž den:** `kCpuHz` = 15 000 000, časovač 0 s maximem 3750
  (1 ms; AAPI volá zpoždění s 100 a 6 na `+DE0C` a `+F5EC`, při starém
  nastavení trvalo 100 „ms“ 5,5 s), tik zvlášť (6.1). Snímání je 72,8×
  za sekundu a držení akordu pět snímání, tedy **69 ms**. Start do
  MAINMENU se zkrátil ze 16,9 na 6,1 s (6.3).

### 6.5 RTC a buzení alarmem

API má `AwokenByRTC` a `EnableAlarm`; `ARIALARM.EXE` a `ARIAMODE.EXE`
přebírají `INT 08h` a `INT 1Bh`. Port RTC zatím neznámý.

**6. 10. 2026: RTC je obvod kompatibilní s MC146818 (DS12887) na I/O
`0500h` + číslo registru.** Doklady (rozbalená 7.05):

- `AAPI.EXE+46F0h`: čte registry 01h, 03h, 05h (alarm sekund, minut,
  hodin); jsou-li všechny `C0h` — v MC146818 „nezáleží“, tedy alarm
  každou sekundu — zapíše je na 0 a přečte registr C (`050Ch`), čímž
  smaže čekající příznaky.
- `ARIALARM.EXE+2132h`: v obsluze `INT 1Ch` čte registr C a testuje bit
  5 = AF (příznak alarmu). Alarm se tedy **dotazuje v každém tiku**,
  nehlásí se přerušením.
- `BKB.COM+1613h`: registr D bit 7 = VRT (baterie a RAM v pořádku).
- `057Ch`, `057Eh`, `057Fh` padají do 128 B paměti CMOS (DS12887): bajty
  nastavení, které přežijí reset. `ARIAMODE.EXE+1361h`/`+137Bh` do
  `057Fh` zapíše `52h` („R“) a do `057Eh` `43h` („C“), nebo 0, podle
  voleb; `TRICKSS2.EXE+06BEh` a `FORMAT.EXE+1E5Fh` se bez „R“ v `057Fh`
  odmítnou nainstalovat, `ATKBD.COM+0599h` bez „C“ v `057Eh` skončí.
  TRICKSS2 běží v AUTOEXEC před ARIAMODE, proto hodnota musí být
  uložená z minula. Význam voleb (karta? klávesnice AT?) zatím neznámý.
  `057Ch` čtou a zapisují ARIACLK, ARIADY, ARIALARM.

Zbývá: který kmitočet má krystal RTC (MC146818 32,768 kHz), zda RTC
budí stroj (AwokenByRTC) a přes jaký pin.

Boot ROM navíc musí mít bajt na `FFFF:000Eh` (na PC modelový bajt BIOSu):
čte ho `AAPI.EXE+46DEh` a přičítá k hodnotě v `[1E68h]`. Hodnota neznámá.

### 6.6 Sériový port

`FOSSIL.COM` přebírá `INT 14h` a používá porty `0600h`, `0601h`, `0681h`
a registry PCB `INT0`/`INT1` (`38h`, `3Ah`) a časovač 1 (`5Ah`, `5Eh`).

### 6.7 Paralelní port a Backpack

`BACKPACK.SYS` přebírá `INT 13h`, `BPHDARIA.SYS` volá `INT 6Bh`.

**Vzpomínka uživatele (6. 10. 2026):** sériový i paralelní port měly na
Arii velmi malé konektory a k plným konektorům se připojovaly přes
**redukce**, které uživatel měl; připojit šlo různý hardware včetně
disketové mechaniky (v provozu ji nikdy neviděl). Sedí s README.DOC
na disku B: (od 702): mechaniky Backpack se připojují kabelem k redukci,
která „converts the Aria's printer port into a full size female plug“,
tiskárna se zapojí za poslední mechaniku, disky dostanou písmena od E:.

**10. 10. 2026.** Oba ovladače jsou od **Micro Solutions** (DeKalb, IL):
`BACKPACK.SYS` („Copyright 1996 by Micro Solutions, Inc., Written by Ron
Proesel“, zařízení `BPDRV$`, tabulka typů mechanik 360 KB až 2,88 MB) je
ve všech verzích od 6.87; `BPHDARIA.SYS` (vlastním jménem „BPHDDRV.SYS“,
1998, hlášky o tabulce oddílů) až od 7.04. README.DOC na B: přitom
uvádí podporu „od 702“ — 6.87 tedy disketový ovladač má, i když ho
dokument nezmiňuje **[ověřit během]**. Protokol paralelního portu zatím
neznámý; stopa k hledání: ovladače `paride` v jádře Linuxu `bpck`
a `bpck6` (Micro Solutions Backpack). `BACKPACK.SYS` sahá i na porty
`0042h`, `0043h`, `0061h` (časovač a reproduktor PC, `BACKPACK.SYS+3600h`
až `+364Bh`; stejnou trojici mají ARIACLK, ARIADY, ARIALARM a CALC, nejspíš
`sound()`/`nosound()` z knihovny Borlandu). Na Arii je ale PCB 80188 na
I/O 0 (6.2), takže `0061h` padne do horního bajtu čítače časovače 2
(`60h`) a `0042h`/`0043h` do vyhrazené části PCB. Zápisy mohou rozhodit
časovač 2 nebo časování ovladače **[předpoklad, ověřit během]**.
Disketa se bude emulovat jako surový obraz 720 KB a 1,44 MB, pevný disk
jako obraz s tabulkou oddílů. Podle README.DOC: písmena od E:, nejdřív
diskety, pak pevné disky, každá skupina podle posledních dvou číslic
sériového čísla; disk nad 2 GB jako dvě jednotky.

**Paralelní port Arie, 11. 10. 2026 (`ar-l5v`, krok 1).** Offsety
v `BACKPACK.SYS` jsou **v souboru** (ovladač zařízení, data i kód od 0);
`tools/dis.py` je ukazuje o 100h výš, jako by šlo o COM — „+3600h“ výše
je tedy v souboru +3500h.

- **Port je na I/O 680h–682h, ne na 378h.** `BACKPACK.SYS` („BP v2.11.AR“,
  sestavení pro Arii) mluví s portem jen přes dva pomocníky: zápis
  (`+18DAh`) jde jen na 680h → data **681h** a 682h → řízení **682h**,
  čtení (`+18FCh`) jen z **681h** (stav) a 682h (vrátí vlastní kopii
  řízení); jiná adresa čte 0. Port bere z BDA jako LPT1, `0040:0008h`
  (`+32FDh`), takže chybějící boot ROM tam dávala **680h**. Nezávisle to
  dokládá obsluha tiskárny v AAPI (`AAPI.EXE+B5C8h`, rozbalený; AH 0/1/2
  jako `INT 17h`, končí `IRET`): znak na 681h, strobe `0Bh` a `0Ah` na
  682h, inicializace `02h` a `0Ah`, stav čte z **680h** — stav tedy jde
  číst na 680h i 681h.
- **Zapojení** z převodních tabulek ovladače (`+19C9h` řízení, 16 B;
  `+18C9h` stav, 256 B; obě čisté permutace bitů): řízení 682h bit 0
  STROBE, 1 SELECT IN, 2 AUTOFEED, 3 INIT (PC bity 0, 3, 1, 2), stejný
  smysl; stav bit 0 PAPER END, 1 BUSY, 2 ACK, 3 SELECT, 4 ERROR (PC bity
  5, 7, 6, 4, 3), BUSY **neinvertovaný** (PC má bit 7 invertovaný).
  Klid je řízení 0Ah = INIT a SELECT IN.
- Emulátor: `src/parallel_port.*` (port Arie, uvnitř převod na PC
  signály pro zařízení za ním), BDA LPT1 = 680h, slovo vybavení 4261h
  (jedna tiskárna). Bez zařízení čte stav jako prázdný port PC (7Fh).
- **Připojení jednotky** (`+1460h`, režim 3, který nastaví `+331Dh`):
  řízení 04h, data = číslo jednotky, SELECT IN dvakrát přepnout (0Ch,
  04h, 0Ch), AUTOFEED nahoru (0Eh), stav bity 3–5 musí být číslo & 7;
  AUTOFEED dolů (0Ch), stav bity 3–5 musí být totéž invertované; pak
  řízení 04h a jednotka je připojená. Ovladač zkouší čísla **0 až 99**
  (`+3349h`) na každém LPT — číslo jednotky jsou zřejmě poslední dvě
  číslice sériového čísla, podle nichž README.DOC řadí písmena.
  Ověřeno stopou portu v dočasném `boot_test` (nezapsaný): při startu
  přesně tato posloupnost pro čísla 0, 1, 2…
- Linux `paride` `bpck.c` (Micro Solutions „backpack“ IDE, 1996–98) má
  podobné připojení (inverzní číslo, číslo, pulzy řízení), ale jiné bity
  a ID na stavu jinak; `ppc6lnx.c` (řada 6) začíná znaky „b“, „p“. Ani
  jeden protokol tohoto ovladače není; slouží jen jako vodítko.
- `FOSSIL.COM` sahá na 681h (mapa hardwaru, „nejspíš UART“) — s datovým
  registrem tiskárny se to musí vyjasnit **[otevřené]**.
- Další krok: co ovladač dělá po připojení — přístup k registrům
  (nejspíš čtení po půlbajtech přes stavové linky) a řadič disket za
  nimi.

**Čip Micro Solutions a řadič disket, 11. 10. 2026 (`ar-l5v`, krok 2,
jen rozbor).** Offsety v souboru, rozebráno `ndisasm` po kusech (data
a kód se v souboru střídají).

- **Primitiva.** Ovladač volá 24 operací portu přes rozdělovač (`+0CE0h`:
  vymění návratovou adresu za položku tabulky za voláním podle `[cs:0E57h]`
  = (režim + 1) & 6). Vstupy po 9 B od `+0CF5h`, pro Arii vždy třetí
  sloupec: `+0CF5h` připojit (`+1460h`), `+0D0Ch` odpojit (`+1507h`),
  `+0D15h` čti registr DL (`+1671h`), `+0D1Eh` zapiš AL do registru DL
  (`+16A9h`), `+0D27h` čti blok (`+16C0h`), `+0D30h` zapiš blok
  (`+16E5h`), `+0D39h`/`+0D42h`/`+0D4Bh` zapiš vnitřní registr 4/5/6 ze
  stínů `[96h]`/`[97h]`/`[98h]`, `+0D54h` vynuluj ukazatele, `+0D5Dh`
  nastav počet, `+0D8Ah` (`+170Dh`), `+0D93h` (`+1764h`) a další.
- **Režim půlbajtů jako Linux `bpck` mode 0:** adresa = data a přepnout
  AUTOFEED (`+1542h`); zápis = data, STROBE nahoru, přepnout INIT,
  STROBE dolů (`+1560h`); čtení = přepnout INIT, stav → dolní půlbajt,
  přepnout INIT, stav → horní půlbajt, převod tabulkou `+17C9h`
  (`+158Bh`); blok čtení/zápisu totéž ve smyčce (`+15D3h`, `+1626h`).
  Adresa **40h + n** = registr n zařízení za čipem; bez 40h vnitřní
  registry čipu.
- **Za čipem je řadič disket PC (rodina 82077), registry 3F0h–3F7h:**
  2 = DOR (reset `08h`, pak `0Ch`, stín `[0A0h]` s motorem a výběrem
  mechaniky; `+2561h`), 4 = MSR při čtení (RQM 80h, DIO 40h před každým
  bajtem; `+26F9h`, `+284Ch`) a DSR při zápisu (`+2299h`), 5 = FIFO
  (příkazy `+2716h`, výsledky `+2874h`), 7 = DIR při čtení (bit 7 výměna
  diskety, `+231Eh`) a CCR při zápisu (rychlost z tabulky `+2AE2h`,
  `+225Ah`). Typy mechanik 360 KB–2,88 MB (`+0AABh`).
- **Data jdou přes vyrovnávací paměť čipu (náhrada DMA):** vnitřní
  registry 28h a 2Ch jsou dva tříbajtové ukazatele (nulují se `+1724h`,
  čtou `+1771h` — rozdíl = bajty k vyzvednutí), 30h tříbajtový počet
  (`+174Dh`), 80h datový port bloku. Zápis sektoru (`+29CCh`): ukazatele
  na 0, blok do bufferu, počet − 1 do 30h, `[96h]` |= 2. Čtení: po
  příkazu čeká na `+0D8Ah` (registr 0Bh bit 7, pak 0Fh) a `+0D93h`
  (registr 0 bit 2, invertovaný), pak čte tolik bajtů, kolik ukazatele
  říkají. Při startu ovladač zapíše do bufferu 256 B a přečte je zpět
  (`+3352h`–`+3375h`): test paměti, který emulovaný čip musí projít.
- Vnitřní registry dosud neobjasněné: 0 (bit 2), 4 (bity 0–3: směr,
  povolení přenosu…), 5, 6 (bity 2, 3 přes `+1A19h`… — nejspíš EEPROM se
  sériovým číslem jako u `bpck`, bitově), 0Bh, 0Fh (čte, zapisuje 18h
  a 10h).
- **Plán emulace:** (1) čip: připojení číslem jednotky, registry
  v půlbajtech, vnitřní registry, buffer, EEPROM se sériovým číslem;
  ověřit stopou, že ho ovladač při startu najde; (2) řadič 82077
  s obrazem diskety 720 KB / 1,44 MB (reset, SPECIFY, RECALIBRATE, SEEK,
  SENSE INTERRUPT, READ ID, READ/WRITE DATA, FORMAT TRACK) a „DMA“ přes
  buffer; (3) nabídka Disketa podle Eureky (složka i obraz).

**Čip Backpack v emulátoru, 11. 10. 2026 (`ar-l5v`, krok 3).**
`src/backpack.*`, `BackpackFloppy` jako zařízení `ParallelPort`.

- Připojení číslem jednotky, půlbajtový přístup k registrům, tříbajtové
  registry 28h/2Ch/30h, buffer 128 KB přes 80h podle kroku 2. Půlbajt
  na stavových linkách: bity 0–3 = PC stav bity 3, 4, 5, 7 (tabulka
  `+17C9h`, jako `j44` v `bpck`).
- **EEPROM 93C46** za vnitřním registrem 6 (bit 0 SK, 1 DI, 2 CS, 3 povolení;
  `+1A19h`–`+1A6Ah`), DO na registru 0 bit 7 (`+1A71h`); slovo čte
  `+33CFh` (start 1, READ 10, šest bitů adresy, 16 hodin, MSB první).
  Co ovladač čte (`+318Ch`–`+3233h`): slovo 0 dolní bajt = NOT horní,
  jinak jednotku odmítne; slovo 1 horní bajt → registr 5; slovo 2
  dolní bajt (< 10h) typ řadiče, 2 = 82077 s DSR (`+2290h`), přes
  tabulku `+365Fh` maska výměny diskety; slova 3–6 pozice mechanik, dolní
  bajt typ (index do jmen `+0AABh`: 7 = „3.5 inch 1.4MB“), FFh žádná —
  **kromě pozice 1 (slovo 4)**, která s FFh dostane typ z parametru
  `DRIVETYPE` (`+31ECh`; hodnota na `+3EDEh`, výchozí 0); slova 37h–3Ah
  sériové číslo, osm číslic ASCII (totéž místo tiskne `bpck`).
  Parametry ovladače: NONSTOP, INTON, DELAY, T7, LASTLPT, DRIVETYPE, A:,
  B:, STACK (`+3EAFh`…).
- Emulátor: slovo 0 = 00FFh, 1 = 0, 2 = 2, slovo 4 = 7, ostatní FFFFh,
  sériové „014187“ + číslo jednotky. **Hodnoty jsou odvozené z toho, co
  ovladač přijme a vypíše, ne z výpisu skutečné jednotky.** S nimi ovladač
  při startu napíše „The following BACKPACK drive is available: Drive E: -
  3.5 inch 1.4MB“ (7.05 i 6.87, týž `BACKPACK.SYS`, MD5 shodné), tedy
  jedna jednotka = E:, jak říká README.DOC. Mechanika je na pozici 1, tedy
  **mechanika 1 řadiče** (výběr v DOR).
- Při startu ovladač na řadiči jen zapíše DOR `0Ch` a přečte DIR; seznam
  mechanik bere z EEPROM.
- `boot_test` připojí jednotku 65 k **druhému** zapnutí (stroj ze snímku)
  a čeká to hlášení. Připojená k prvnímu stroji na 6.87 (ne 7.05)
  způsobila, že pozdější vložení prázdné karty DLFFSA nepoznal (bez
  zkušebních zápisů, vytvoření souboru „uspělo“ z bufferů DOS) — týž
  druh křehkosti 6.87 jako `ar-z2e`, zapsáno tam.
- Další krok: řadič 82077 s obrazem diskety a přenos přes buffer čipu.

### 6.8 PCMCIA

`CARDMAN.EXE` (PKLITE); `TRICKSS2.EXE` čte `057Fh`, `ATKBD.COM` `057Eh`.

**6. 10. 2026.** `TRICKSS2.EXE` je CardTrick (Datalight 1993–94) se
**Socket Services 2.01 „SS2OEM v1.1 (c) 1995–1997 Robotron“** — vrstva
Robotronu pro slot Arie. `DLFFSA.EXE` je flash souborový systém k ní
(„CardTrick Stack was reentered“). Obě mají stejný kód hardwaru:

- `0480h` při čtení je **stav slotu**: bity 0–1 detekce karty, **aktivní
  nulou** (`TRICKSS2.EXE+14C1h`: obě 0 = karta zasunutá); bity 2–3
  nejspíš BVD1/BVD2, napětí baterie karty (`BKB.COM+1622h` porovnává
  s `0Ch`); bit 5 další stav (`TRICKSS2.EXE+14B3h`; ochrana proti zápisu?).
  Při zápisu je `0480h` vyšší polovina stránky okna (6.2).
- `0702h` napájení slotu: obsluha `INT 1Ch` (`TRICKSS2.EXE+14EDh`,
  `DLFFSA.EXE+0DA3h`) zapíše **6, když je karta zasunutá, a 4, když
  není** (oprava: dřív zde stálo obráceně); kolem zápisu `7` a pak `6`
  (`TRICKSS2.EXE+1549h`). Výklad: bit 0 programovací napětí Vpp, bit 1
  napájení karty VCC, bit 2 vždy 1 **[předpoklad]**.
- Okno karty je režim 3 `INT 69h` (`TRICKSS2.EXE+1497h`, `DLFFSA.EXE+0E92h`).
- Při dlouhých operacích cvaká `INT 61h AX=0500h` (`DLFFSA.EXE+0EE6h`),
  to je to „tikání“ z README.DOC.
- **Vzpomínka uživatele:** používaly se dva druhy karet — starší **flash**
  (pomalejší, menší, data drží napořád) a **SRAM** (větší, rychlejší, data
  drží vestavěná vyměnitelná baterie). Sedí s hláškami formátování
  (`IDS_FM_FORMAT_CARD_TYPE_SRAM`, `_FLASH_1`, `_FLASH_2`, `_ROM`), s bity
  BVD v `0480h` a s hláškami `IDS_KBD_PCCARD_LOW/FLAT` (baterie SRAM
  karty, 6.4). Volba MODE „old PC card“ by mohla souviset se starším
  typem flash karet **[předpoklad]**.
- **Vnější klávesnice:** `ATKBD.COM` je ovladač klávesnice IBM/AT, který
  čte přes `INT 14h` — tedy přes sériový port, zřejmě s převodníkem;
  zapíná ho volba MODE „IBM keyboard“ (CMOS `057Eh` = „C“, 6.14).
  Uživatel ji nikdy nepoužil a neví, jak se připojovala.

**Rozbor karet, 10. 10. 2026** (požadavek uživatele: trvalé úložiště,
karty flash i SRAM a mechaniky Backpack, z pohledu uživatele jako
diskety v emulátoru Eureky).

*„Stará fleš karta“ je formát a ovladač, ne jiný druh karty.* Na disku
B: jsou **dva celé zásobníky** Datalight a oba jsou ve všech verzích,
které máme (6.87, 6.90, 6.99, 7.04, 7.05):

- **Nový:** `DLFFSA.EXE` z CONFIG.SYS (CardTrick 3.01.13, Datalight
  1993–96) a formátovač `DLFMT.EXE` (CardTrick 3.01.19, řetězec
  `FTL100`). Flash karta má na sobě **FTL** (Flash Translation Layer),
  DOS vidí FAT nad ním. Hláška DLFMT „The installed driver is configured
  to only support up to %dMB flash cards“.
- **Starý:** `TRICKSS2.EXE` z AUTOEXEC (CardTrick 1993–94, Socket
  Services Robotronu SS2OEM, řetězec „Datalight FlashDisk 1.0“)
  a formátovač `CARDMAN.EXE` („Enter Disk Type (D=DOS-FAT,
  F=Flash-FAT)“, „Format card with a CIS“, typ 1 SRAM / 2 FLASH,
  velikost karty se zadává ručně). Nainstaluje se jen s „R“ v CMOS
  `057Fh`, tedy se zapnutou volbou MODE „podpora staré fleš karty“
  (výše a 6.14). Hlášky MESSAGES.DAT: „p c karta, starý formát fleš
  karty“, „použít starý formát fleš karty“, „nemůže formátovat více než
  čtyři megabajty se starým formátem fleš karty“ a „please disable old
  flash card support in mode“.
- `FORMAT.EXE` („ARIA formatter, rev 1.5a, 1996–1999“) volí mezi nimi:
  řetězce `%Fscardman <%s >%s` a `%Fsdlfmt %c: /c > %s`, nabídka
  jednotek „PC CARD“, „Old PC CARD“, „BackPack“. `FORMAT.EXE+1E5Fh` se
  bez „R“ v `057Fh` vrátí (cesta staré karty).

Výklad: karty uživatele mohly být fyzicky tytéž flash karty; rozhoduje,
**jakým formátem byly naformátované**. Karta ze starého formátu se
s vypnutou volbou nejspíš nepřečte a naopak **[předpoklad]**. Zda oba
ovladače běží zároveň (DLFFSA je rezidentní dřív, TRICKSS2 má hlášku
„CardTrick already resident“), ověří až běh s kartou.

*Hardware slotu, doplnění z `FORMAT.EXE` (rozbalený, 7.05):*

- `FORMAT.EXE+1DA9h` mapuje 32bitovou adresu na kartě: bity 16–19 do
  `0580h` bitů 2–5 s režimem 3, bity 20–25 do `0480h` bitů 0–5 a **bit 7
  `0480h` = 1 společná paměť, 0 paměť atributů** (třetí argument ≠ 0 =
  atributy). To je signál REG# slotu; okno 64 KB na D000h, adresovatelných
  64 MB. Sedí s režimem 3 `INT 69h` (6.2), kde bit 7 dává `CL=0`.
- **`0480h` bit 5 = ochrana proti zápisu**, doloženo:
  `FORMAT.EXE+06BAh` při nastaveném bitu řekne
  `IDS_FM_FORMAT_CARD_WRITE_PROTECTED` (666h). Tím padá „?“ výše.
- `0480h` bity 0–1 obě 0 = karta zasunutá (`FORMAT.EXE+1E00h`).
- `0702h` při formátování: 7 (s Vpp) a pak 6 (`FORMAT.EXE+1E13h`, `+1E1Dh`).
- **Rozpoznání typu** (`FORMAT.EXE+2954h`, cesta staré karty):
  1. na adrese 0 společné paměti zapíše negovaný bajt a čte zpět; když
     drží, je to **SRAM** (typ 0) a velikost zjistí `+25D0h`;
  2. jinak porovná 32 bajtů paměti atributů s 32 bajty společné; když se
     shodují (karta nemá zvlášť atributy, tedy ani CIS), zkusí flash
     příkazy (`+274Ch`);
  3. jinak čte CIS: první n-tice `01h` (CISTPL_DEVICE), typ zařízení
     5–7 a z kódu velikosti spočte kapacitu; jiná karta je typ 4.
- **Flash má příkazy Intel**, zapisované slovem na dva prokládané čipy:
  `FFFFh` (čtení pole) a `9090h` (čtení identifikace), `FORMAT.EXE+27E9h`
  až `+2863h`. Které kódy čipů zná a z čeho vzniká „fleš 1“ a „fleš 2“,
  je otevřené; druhá cesta (`FORMAT.EXE+076Bh`, údaje z ovladače přes
  `INT 2Fh` AX=2F14h/2F01h) dává „fleš 1“, když je nějaká 32bitová
  hodnota větší než 20000h **[význam neznámý, snad velikost bloku]**.
- `FORMAT.EXE+02C3h` po formátu přes CARDMAN namapuje stránku 0
  společné paměti a opraví v zaváděcím sektoru na D000:0015 bajt média
  F0h na F8h a na D000:0013 počet sektorů zaokrouhlí nahoru na násobek
  128. Karta typu DOS-FAT (SRAM) tedy nese **obyčejný svazek FAT od
  bajtu 0** společné paměti — takovou kartu by šlo ukázat i ze složky
  hostitele jako disketu v Eurece; flash (FTL i Flash-FAT) ne.
- Programy, které porty slotu používají přímo: `DLFFSA.EXE`,
  `TRICKSS2.EXE`, `FORMAT.EXE` (`0480h`, `0580h`, `0702h`, `057Fh`);
  `BKB.COM` a `MAINMENU.EXE` čtou `0480h` (baterie karty).

*Návrh postupu (beady pod `ar-1ln`):* karta se emuluje **na úrovni
hardwaru** (okno v režimu 3, REG#, `0480h`, `0702h`) a originální
ovladače běží beze změny. Obsah karty je **surový obraz** v souboru
hostitele: FTL ani Flash-FAT nejdou namapovat na složku, jak to dělá
Eureka. Pořadí: (1) karta SRAM — lineární paměť, BVD a ochrana proti
zápisu; (2) uživatelská vrstva podle Eureky (vložit, vysunout, nová
karta, sloty, titulek, tóny); (3) flash karta s příkazy Intel, oba
formáty; (4) Backpack (6.7). RAM disk D: už mezi spuštěními přežívá
(`ar-ram`, 6.9).

**Rozhodnutí uživatele, 10. 10. 2026:** pořadí platí. Karta SRAM se
bude dát vložit **ze složky hostitele** (hlavní cesta, jak dostat
soubory z PC do Arie; dnes žádná není), obraz karty jako druhá možnost.
Složka se při vložení přeloží na svazek FAT (s podadresáři, jména 8.3
v kódování obrazu B: z `FlashImage`) a při vysunutí a ukončení zapíše
zpět — stejná logika jako diskety Eureky (`virtual_disk.*`), jen nad FAT
místo CP/M. Flash karta zůstane obrazem: emulátor ji vyrobí prázdnou
a naformátuje ji Aria sama; soubory se na ni dostanou kopírováním
v Arii (karta SRAM ze složky → RAM disk D: → výměna karty → flash).
Na skutečné Arii se data přenášela **sériovým kabelem** (čtečka karet
pro PC nebyla), proto má přijít i přenos po sériové lince (6.6).

**Karta SRAM v emulátoru, 10. 10. 2026 (`ar-3ap`, krok 1).**
`src/pc_card.*`: karta SRAM jako lineární paměť, nad svou velikostí se
zrcadlí (FORMAT.EXE+25D0h zjišťuje velikost právě přetočením po 64 KB),
paměť atributů čte `FFh` (žádný CIS). `AriaMachine` mapuje režim 3 okna
na kartu podle `FORMAT.EXE+1DA9h`, `0480h` vrací detekci, BVD a ochranu
proti zápisu; `InsertCard`/`EjectCard`. `FormatFat12` (`src/fat12.*`)
vyrobí prázdný svazek FAT12 (používá ho i náhradní ROM disk A:).

Doloženo během (`boot_test`, 7.05 i 6.87): s naformátovanou kartou
512 KB se hlásí `DLFFSA` „Drive(s) C“ a zároveň `TRICKSS2` „Card
Drive(s) E:“ (stará podpora je v CMOS zapnutá). Když MAINMENU čeká na
klávesu, test zavolá v emulované Arii `INT 21h`: vytvoří, zapíše
a přečte `C:\KARTA.TXT`, a soubor pak najde čtečka FAT přímo v bajtech
karty. Mutace (karta zahodí zápisy) test shodí už na vytvoření souboru.

Zjištěno mimochodem: **TRICKSS2 si bere písmeno E:**, které README.DOC
přisuzuje první mechanice Backpack — se zapnutou starou podporou karet
by Backpack nejspíš dostal až F: **[předpoklad, ověřit s `ar-l5v`]**.
Při tom se ukázal i souběh v `run-tests`: oba běhy `boot_test` sdílely
dočasný soubor snímku paměti; teď má jméno podle CRC obrazu.

Baterie karty: `BKB.COM+1622h` při zasunuté kartě a bitech 2–3 ≠ 11b
vybere hlášku podle bitu 2 (`[2BFh]` při bitu 2, jinak `[2C1h]`).
Emulátor dává slabé baterii jen bit 2 (BVD1) a vybité žádný, podle
zvyklosti PCMCIA **[předpoklad]**; která hláška je LOW a která FLAT,
neověřeno. **Ověřeno týž den** (krok 3 níže): mapování sedí.

**Kapacity karet, 10. 10. 2026.**

- **Vzpomínka uživatele:** jeho flash karta byla na **E:**, C: skoro
  nepoužíval. E: je písmeno `TRICKSS2` (výše), takže karta byla ve
  **starém formátu** (Datalight FlashDisk 1.0, Flash-FAT) se zapnutou
  starou podporou v MODE.
  **Neplatí (oprava týž den):** uživatel našel zálohu své složky
  „Ariacard“ z doby používání; soubory INI v ní ukazují na **C:**
  a starou podporu karet **vypnutou**. Hlavní karta byla tedy flash
  v **novém formátu FTL** (DLFFSA), data držela trvale bez baterie;
  karty SRAM jen zkoušel. Vzpomínka na E: byla mylná.
- **Hardware:** okno adresuje 26 bitů (6 bitů `0480h` + 4 bity `0580h`
  + 16 bitů offsetu), tedy **64 MB**; stejnou mez má zkouška velikosti
  SRAM ve `FORMAT.EXE+25D0h` (nejvýš 400h kroků po 64 KB).
- **SRAM přes DLFFSA, změřeno:** dočasná úprava `boot_test` (nezapsaná)
  s kartami 0,5–64 MB naformátovanými `FormatFat12`: DOS přes `INT 21h`
  AH=36h vidí celou kartu (64 MB: 2 047 alokačních jednotek po 32 KB,
  65 504 KB) a zápis 16 B na pozici 64 MB − 256 KB našla čtečka FAT
  v bajtech karty na správném místě. SRAM tedy funguje **až do 64 MB**;
  jaké velikosti se skutečně prodávaly, nevíme (u karet SRAM té doby
  obvykle do jednotek MB **[předpoklad]**).
- **Starý formát flash:** nejvýš **4 MB** (hláška `FORMAT.EXE`, výše).
- **Nový formát FTL:** mez je v konfiguraci ovladače; `DLFMT.EXE` se na
  ni ptá za běhu (`DLFMT.EXE+1123h`–`+1141h` vypíše „support up to
  %dMB flash cards“ s hodnotou z `[160Ch]`), v souboru napsaná není.
  Zjistí se během s flash kartou (`ar-ibv`).

Pro uživatelskou vrstvu (`ar-aj1`): karta ze složky má pevnou velikost
zvolenou při založení, jako skutečná karta (zápisy z Arie ji pak
nepřeplní, FAT má danou velikost). Když složka při vložení přesahuje,
emulátor to řekne i s potřebnou velikostí a kartu nevloží — jako
diskety Eureky.

Zbývá v `ar-3ap`: napájení slotu `0702h` (okno teď funguje bez ohledu na
něj), vysunutí za běhu (detekce výměny v DLFFSA), test ochrany proti
zápisu a slabé baterie (hlášky BKB).

**Výměna karty za běhu, 10. 10. 2026 (`ar-3ap`, krok 2).** Vysunutí za
běhu tím přestává být otevřené. `boot_test`: karta A s `KARTA.TXT` ven,
slot prázdný, karta B dovnitř; DOS pak nesmí vidět soubor z A, nový
soubor musí přistát na B a bajty vytažené A se nesmějí změnit. Pak
zpět na A bez čekání.

- Se slotem prázdným **30 ms** DOS výměnu nepoznal a dál pracoval
  s adresářem předchozí karty (z bufferů, `buffers = 50`); od **60 ms**
  (0,06; 0,12; 0,25; 0,5; 1 s) vždy poznal. DLFFSA tedy bere výměnu
  z prázdného slotu, který uvidí v obsluze `INT 1Ch` (jeden tik 55 ms).
- Proto `AriaMachine` drží slot prázdný sám: karta vložená dřív, než
  prázdný slot prožil `kCardSwapTicks` = 5 tiků (asi 0,27 s), čeká
  mimo slot (`CardWaiting`). Počítají se **tiky, ne čas**: test hledá
  bezpečné místo krokováním CPU bez tiků a čas měřený cykly tam uplynul,
  aniž by ovladač slot viděl. Při zapnutí jde čekající karta rovnou
  dovnitř.
- Mutace (vložení vždy hned) test shodí.

Zbývá: vysunutí uprostřed zápisu emulátor nehlídá (karta se vytáhne
mezi dvěma úseky běhu, jako by ji vytáhla ruka); uživatelská vrstva
(`ar-aj1`) může vysunutí odložit, dokud okno není v režimu 3 **[návrh]**.
Dál napájení `0702h`, ochrana proti zápisu a baterie.

**Napájení, ochrana proti zápisu a baterie karty, 10. 10. 2026
(`ar-3ap`, krok 3).**

- **`0702h` bit 1 = VCC karty** (`DLFFSA.EXE+0DD9h`–`+0DEDh`: obsluha
  `INT 1Ch` při změně detekce zapíše 6 pro kartu, 4 bez ní; 7, tedy bit 0
  = Vpp, jen kolem programování flash, `+0D58h`/`+0D84h`). Emulátor
  kartu bez bitu 1 nevidí (čte `FFh`, zápisy zahodí) a při zapnutí
  latch nuluje; start 7.05 i 6.87 se všemi zkouškami karty projde, takže
  ovladače kartu napájejí samy. Bit 2 (vždy 1) zůstává nevyložený.
- **Ochrana proti zápisu** (`boot_test`): vytvoření souboru na chráněné
  kartě — AAPI ohlásí „c chráněn proti zápisu“ a čeká na r / f / i
  (zkusit znovu, zrušit, ignorovat; MESSAGES.DAT). Test stiskne f (body
  1, 2, 4 = `0581h` `0Eh`), DOS vrátí chybu 5 a bajty karty se nezmění.
- **Baterie karty** (`boot_test`, hlášení baterie mezera + body 12 =
  `0581h` `86h`, `K_BATTERY`): dobrá → jen „plně nabitá“ (baterie
  Arie); bit 2 sám → „…, p c karta částečně vybitá“; ani jeden →
  „…, p c karta zcela vybitá“. Mapování PCMCIA tím je doložené.

`ar-3ap` je tím hotový. Vysunutí uprostřed zápisu zůstává jako návrh pro
`ar-aj1` (výše).

**Karta ze složky, 10. 10. 2026 (`ar-aj1`, krok 1).** `src/card_folder.*`
(`FolderCard`) podle disket Eureky (`virtual_disk.*`), ale nad FAT12
s podadresáři:

- `Load` převede složku i s podsložkami na obraz karty zvolené velikosti
  (64 KB až 64 MB, mocnina dvou). Jména velkými písmeny 8.3 v kódování
  obrazu B: (Kamenický nebo CP852), takže česká písmena, která kódová
  stránka má, zůstanou; mezery zmizí, nepovolené znaky jsou `_`, dlouhé
  nebo kolidující jméno dostane `~1`, `~2`. Kořen má 512 položek (maximum
  DLFMT), jmenovka svazku je jméno složky podle pravidel Arie
  (README.DOC na B:, „VOLUME LABELS“: nejvýš 11 znaků, mezera smí, znaky
  `* ? / \ | . , ; : + = [ ] ( ) & ^ < > " '` se vypustí, víc mezer za
  sebou se sleje), datum a čas ze souborů
  hostitele, příznak jen pro čtení se přenese. Když se složka nevejde,
  `needed` řekne nejmenší kartu, která stačí (hláška v KB).
- `Save` zapíše kartu zpět: soubor, který karta má, zůstane pod jménem
  hostitele, odkud přišel, a zapíše se jen při změně obsahu (FNV-1a);
  nový soubor dostane jméno 8.3 dekódované z kódování; smazaný se
  přesune do `.aria-trash` ve složce (i s cestou), složka se smaže, jen
  když je prázdná. `.aria-trash` se na kartu nikdy nedává.
- `card_selftest` (bez dat, 18. řádek PASS): obě kódování, kolize, prázdný
  soubor, podsložka, soubor přes mnoho clusterů, zápis zpět (změna,
  ponechání, nový soubor i složka, koš), opakované uložení a odmítnutí
  příliš velké složky s nabídkou 1024 KB. Mutace (bez koše) ho shodí.
- `boot_test` teď vkládá kartu ze složky: Aria (DOS přes `INT 21h`)
  přečte `C:\DOPISY\PRVNID~1.TXT` ze souboru „Dopisy\Prvni dopis.txt“
  a soubor `KARTA.TXT`, který zapsala, se po `Save` objeví ve složce.

Omezení: jen FAT12 — kartu, kterou Aria přeformátuje na FAT16 (DLFMT to
umí), `Save` odmítne a složku nechá být **[ověřit, zda FORMAT na SRAM
FAT16 dělá]**. Zatím se ukládá jen na výslovný `Save`; průběžné ukládání
po utišení karty (jako Eureka po sekundě klidu) přijde s nabídkou.

**Nabídka Karta, 10. 10. 2026 (`ar-aj1`, krok 2).** Nabídka Karta je
druhá za Souborem, jako Disketa v Eurece (pořadí upraveno na přání
uživatele), se zkratkami Disketa z Eureky: `Ctrl+I` vložit ze složky, `Ctrl+0` vysunout,
`Ctrl+Z` zamknout; tóny diskety z Eureky; titulek „karta <složka>,
<velikost>[, zamčená]“, jinak „bez karty“. Dialog `IDD_CARD_SIZE` nabídne
jen velikosti, na které se složka vejde (`FolderCard::FittingSizes`),
s předvolbou nejmenší od 1 MB; popisek seznamu nese, kolik složka
zabírá, aby ho NVDA přečetla se seznamem. `win/dialog.*` převzaté
z Eureky beze změny. `EmulatorThread::Call` spustí úlohu na vlákně
emulátoru mezi úseky a počká na ni (vložení, vysunutí, zámek). Vysunutí,
které se do složky nepodaří zapsat, vrátí kartu do slotu a řekne to;
při ukončení se karta zapíše po zastavení vlákna, ať se s pamětí stane
cokoli. Zkratka bez karty pípne (`MessageBeep`), nemlčí.

Ověřeno: sestavení bez varování, 18 PASS, řetězce v EXE v UTF-16.
**Neověřeno v okně** — ovládání s NVDA zkusí uživatel.

**Oprava atributů, týž den (nahlásil uživatel po zkoušce v okně).**
Atributy nastavené v Arii se do složky nedostaly a z Windows šel na kartu
jen příznak jen pro čtení. Příčiny: `Save` porovnával jen obsah, takže
změna samotných atributů se přeskočila, a ani přepsaný soubor atributy
z karty nedostal. Teď nesou oba směry čtení, skrytý, systémový a archiv
(01h, 02h, 04h, 20h — stejné bity ve FAT i ve `FILE_ATTRIBUTE_*`),
podsložka skrytý a systémový; ostatní bity hostitele zůstanou. Skrytý
nebo systémový soubor Windows běžným vytvořením nepřepíše, proto se před
zápisem atributy shodí a po něm nastaví z karty. `card_selftest` to
zkouší (mutace „změna atributů se ignoruje“ shodí tři kontroly). Že
změny ve Windows uvidí Aria až po novém vložení, je záměr: karta je
kopie z chvíle vložení (README). Další krok:
karty z obrazu, nová prázdná karta, průběžné ukládání a zapamatování
karty mezi spuštěními; pak sloty.

Uživatel nabídku vyzkoušel v okně s NVDA; kromě atributů vše fungovalo.

**Průběžné ukládání a nastavení, 10. 10. 2026 (`ar-aj1`, krok 3).**

- `PcCard::Writes()` počítá změněné bajty. Časovač okna (500 ms) se přes
  `EmulatorThread::Call` dívá na počet; když se po změně dva pohledy
  nehne (sekunda klidu, pravidlo Eureky), okno zkopíruje obraz a zapíše
  ho do složky. Příznak změny se smaže jen po úspěchu a jen když Aria
  mezitím nepsala. Po neúspěchu hlášení jednou, další pokus až po další
  změně (a při vysunutí, které chybu hlásí samo).
- `src/settings.*` (podle `Settings` z Eureky: key=value v UTF-8 s BOM,
  zápis přes dočasný soubor) v `nastaveni.txt` vedle souboru paměti:
  `posledni-karta=` a `kartaN=<bajty>;<0|1 zámek>;<složka>`. Velikost
  a zámek patří kartě (složce), jako u Eureky zámek disketě; při vložení
  je zapamatovaná velikost v dialogu předvybraná a zámek platí. Karta ve
  slotu při ukončení se při startu vloží znovu (`InsertRememberedCard`,
  před spuštěním vlákna, bez tónu); vysunutí zapamatovanou kartu smaže.
  Složka, která zmizela nebo přerostla kartu, se ohlásí a zapomene.
- `card_selftest`: nastavení tam a zpět (diakritika i `;` v cestě, jiná
  velikost písmen = táž karta). Průběžné ukládání samo test nemá —
  logika sedí v okně; ověří ho uživatel.

**Poškozená karta při zkoušce uživatele, týž den (otevřené).** Složka
`D:\aria\Karty\ARIACARD` (23 souborů, 660 KB) jako karta 16 MB. V AWP
uživatel vytvořil `POKUS.TXT`; Aria ho viděla, složka ne. Průběžné
ukládání pak hlásilo, že `SYMDEB.EXE` je na kartě poškozený, a v Arii měl
SYMDEB 3,9 MB místo 25 KB, ač na něj uživatel nesáhl. Výklad: položka
SYMDEB leží ve stejném sektoru kořenového adresáře (položky 16–31) jako
nová `POKUS.TXT`; DOS sektor přečte, doplní položku a zapíše celý zpět,
takže zápis špatné nebo zastaralé kopie toho sektoru poškodí sousední
položku.

- **Nereprodukováno:** dočasný `boot_test` (nezapsaný) s kopií té složky
  na 16 MB: DOS přes `INT 21h` vytvoří, zapíše a přečte soubor a `Save`
  projde. Rozložení karty samo je tedy v pořádku; rozdíl je v AWP.
- **Kandidát:** se zapnutou starou podporou (výchozí „R“ v CMOS
  emulátoru) slouží tutéž kartu i `TRICKSS2` jako **E:** — DOS přes E:
  otevře `SYMDEB.EXE` a vytvoří soubor, karta se změní na čekaném místě
  (20 B, položka kořene). Dva ovladače se dvěma sadami bufferů nad jednou
  kartou mohou zapsat zastaralý sektor. Uživatel měl na skutečné Arii
  starou podporu vypnutou.
- **Druhý kandidát:** přerušení (řeč, AAPI) přemapuje okno D000h na
  flash uprostřed přenosu DLFFSA; test bez souběžné řeči to nepozná.
- **Opraveno hned:** `FolderCard::Save` kvůli jednomu nečitelnému souboru
  vzdal celé uložení, proto se `POKUS.TXT` do složky nedostal. Teď
  nečitelný soubor přeskočí (kopie ve složce zůstane, nejde do koše),
  zbytek zapíše a ohlásí poškozené soubory; okno je řekne jednou a obraz
  karty uloží jako `karta-poskozena.img` vedle nastavení pro rozbor.
  `card_selftest` to zkouší s položkou, která tvrdí 3,9 MB.
- Dál: obraz karty od uživatele a přesný postup v AWP.
- **Doplněno:** uživatel chybu s novým sestavením nezopakoval. Druhý
  kandidát zkoušen dočasným `boot_test` (nezapsaný): šest kol „kde jsem“
  (řeč běží, vzorky přibývají) a v každém čtyři soubory po 3 000 B přes
  `INT 21h` na kartu 16 MB z jeho složky — 24 souborů, žádná chyba DOS,
  `Save` bez poškozených souborů. Přemapování okna během řeči tím
  nepotvrzeno. Zůstává kandidát E: (dva ovladače) a cokoli specifického
  pro AWP. Diagnostika (`karta-poskozena.img`, hlášení poškozených
  souborů) zůstává v sestavení, aby další výskyt přinesl obraz.

**Karty z obrazu a nová karta, 10. 10. 2026 (`ar-aj1`, krok 4).** Slot
nese kartu se dvěma druhy domova: složku (`FolderCard`) nebo **obraz**,
soubor s běžnou pamětí karty bajt po bajtu (`src/card_image.*`).

- Obraz se čte a zapisuje celý, zápis přes dočasný soubor a přejmenování;
  průběžné ukládání, vysunutí i ukončení jdou stejnou cestou
  (`MainWindow::SaveCard`). Hlášení poškozených souborů a
  `karta-poskozena.img` platí jen pro složku.
- Obraz jiné velikosti než mocniny dvou 64 KB–64 MB se **odmítne**, ne
  doplní: doplnění by soubor uživatele při prvním uložení zvětšilo a
  `FORMAT.EXE+25D0h` měří kartu zrcadlením (výše „Kapacity karet“).
- `nastaveni.txt` beze změny tvaru: řádek `kartaN` nese složku i obraz,
  druh rozhodne, zda je cesta složka. Starší soubory se čtou stejně
  (`card_selftest`).
- Nabídka Karta: `Ctrl+O` z obrazu (Eureka obrazy nemá), `Ctrl+M` nová
  karta jako „Nová disketa“ Eureky — dialog s druhem (prázdná složka,
  naformátovaný obraz, nenaformátovaný obraz) a velikostí, pak výběr
  cesty dialogem Windows. Naformátovaný obraz dělá
  `FolderCard::Formatted`, tedy totéž rozvržení jako karta ze složky.
  Nenaformátovaný je samé FFh (smazaná flash; i flash karty `ar-ibv`
  budou obrazy).
- Opraveno mimochodem: vložení téže složky, která už je ve slotu, ji
  dřív přečetlo **před** zápisem karty zpět, takže by Aria dostala
  zastaralou kopii. Teď se karta nejdřív vysune (`EjectIfInSlot`).
- **Prázdná karta v Arii, změřeno v `boot_test` (7.05 i 6.87):** vytvoření
  souboru na kartě samých FFh hned vrátí chybu DOS 4, bez otázky
  kritické chyby a bez řeči; ovladač kartu zkusmo přepíše a vrátí bajty
  zpět (`Writes` 34, obsah dál FFh). Stroj tedy nezamrzne. **Neověřeno:**
  že ji `FORMAT.EXE` (spouští ho `FILEMGR.EXE`, řetězec `%Fs\format.exe`)
  v emulátoru opravdu naformátuje — ověří uživatel, případně `boot_test`
  se spuštěním FORMAT.
- Další krok: sloty `Ctrl+1` až `Ctrl+9`.
- **Zkouška uživatele týž den:** formátování v Arii funguje na všech
  druzích karet. Správce souborů na nenaformátované kartě ukázal žádné
  soubory a kapacitu kolem 2 012 KB, po naformátování přes 16 000 KB
  (karta 16 MB). Dočasný `boot_test` (nezapsaný), `INT 21h` 36h na kartě
  16 MB samých FFh: C: hlásí 1 sektor na cluster, 1 010 clusterů
  (505 KB), volno 0 — náhradní geometrie ovladače pro kartu bez BPB, ne
  velikost karty; naformátovaná 16 368 KB na C: i E:. Táž otázka na **E:**
  (TRICKSS2) u prázdné karty: AAPI řekne „celkové selhání e“ a čeká na
  klávesu, `f` ji v testu neukončilo. Obojí dělají původní ovladače; chyba
  emulátoru tím není doložená.
- Dialogy obrazů se otvíraly na ploše: shell si poslední složku
  nepamatoval. Teď se otvírají ve složce posledního obrazu
  (`slozka-obrazu` v `nastaveni.txt`), jinak vedle karty ve slotu nebo
  poslední karty; tam se otvírá i dialog nové složky (`StartIn`
  v `win/dialog.cpp`).
- **Otevřené (`ar-pdw`):** správce souborů v `B:\SYSTEM`
  neukáže žádné soubory, `DIR B:\SYSTEM` v DOSu ano. Soubory tam mají
  obyčejné atributy (20h, `LPC.DSP` 21h), takže je neskrývá atribut.
  `BrowseDirInit` (API.DOC) alokuje seznam souborů z globální paměti
  AAPI; v `SYSTEM` je 38 souborů a podsložka, víc než jinde — podezření
  na nedostatek paměti v emulátoru, neověřeno.
  **Už neplatí:** složka `src` repozitáře (přes 60 souborů) na kartě se
  procházela správně a hned potom i `B:\SYSTEM`. Chyba tedy závisí na
  stavu, ne na počtu souborů. K tomu „označit vše“ v té složce ohlásilo
  miliony označených souborů. Zda je to chyba firmwaru, nebo emulátoru
  (např. paměť, kterou emulátor nuluje a skutečná RAM ne), není
  rozhodnuto.
  **Miliony rozhodnuty, týž den: chyba firmwaru 7.05.** Postup uživatele
  (F7, šipky dolů+vlevo na C:, nahoru+vpravo do `src`, Shift+F3 označit
  vše, Shift+F8 stav) zopakován dočasným `boot_test` (nezapsaný) s kopií
  jeho karty. Stav čte zprávy 630h–634h z `MESSAGES.DAT`; v 7.05 je 633h
  „%lu označených souborů“ (32 bitů), ale `FILEMGR.EXE+5B8Dh` (rozbalený)
  uloží jen 16bitový počet (`push word [bp-1Ah]`, pak 633h). Horní slovo
  si formátovací funkce vezme ze zásobníku, kde zůstalo po předchozím
  volání, takže počet vyjde o násobek 65 536 větší. Sousední 632h
  „%lu volných kilobajtů“ dostává 32 bitů (`push dx`, `push ax`) a je
  správně. V 6.87 je zpráva „%u označených souborů“ a test řekl správně
  „39 označených souborů“. Skutečná Aria se 7.05 by tedy hlásila totéž;
  emulátor to neopravuje (firmware se nemění). Řečový log `boot_test`
  zachytí jen `INT 61h` AX=0200h/0201h; řádky 631h a 633h v něm v 7.05
  chyběly, ač je Aria říká — log není úplný, proč, otevřené.
  Zbývá z `ar-pdw` jen prázdný `B:\SYSTEM`.

**Sloty, 10. 10. 2026 (`ar-aj1`, krok 5).** Devět slotů `Ctrl+1` až
`Ctrl+9` a dialog „Sloty s kartami“ (Karta → Spravovat sloty) podle
Eureky, zjednodušené: každá karta tu má složku nebo soubor, takže
neuložená karta „*pamat“ z Eureky nemá protějšek.

- `nastaveni.txt`: `slotN=cesta`. Zámek patří kartě (řádek `kartaN`),
  ne slotu; zámek nastavený v dialogu u složky, která kartou ještě nebyla,
  se zapíše s velikostí 0 = neznámá, a taková složka se při vložení ze
  slotu zeptá na velikost.
- Slot se složkou známé velikosti nebo s obrazem vloží kartu hned; prázdný
  slot a zmizelá cesta se ohlásí se jménem slotu.
- Položky slotů na konci nabídky Karta nesou jméno karty (přepisuje je
  `RefreshSlotItems` při otevření nabídky) a mají předponu „F11, “ jako
  ostatní zkratky.
- Změna zámku v dialogu u karty, která je ve stroji, se na ni přenese
  s tónem (`ToggleCardLock`).
- Volba slotu v dialogu Nová karta (Eureka ji má) zatím chybí.
- `card_selftest`: sloty a zámek bez velikosti tam a zpět. Dialog a
  vkládání ze slotu ověří uživatel. **Ověřeno uživatelem** týž den.

**Karta zamčená od zapnutí a CIS, 10. 10. 2026.** Uživatel zamkl kartu
HRY (1 MB, zapamatovaná, tedy ve slotu už při zapnutí); správce souborů
(F7, na C: šipky dolů a vlevo naráz) i F8 pak na každý pokus řekly
„celkové selhání c“, i po teplém resetu. Stará podpora byla vypnutá.

- Reprodukováno dočasným `boot_test`: karta zamčená **od zapnutí** →
  „celkové selhání c“; zamčená až po startu, nebo odemčená od zapnutí →
  v pořádku. Odemčenou kartu ovladač při startu zkusmo přepíše (66 B)
  a vrátí; zamčenou ne.
- Výklad podle rozpoznání typu (`FORMAT.EXE+2954h`, výše): zápis zkušebního
  bajtu neprojde, paměť atributů (dřív samé FFh) se liší od společné,
  tedy se čte CIS — a ten chyběl, takže karta byla „jiná“. Skutečné karty
  SRAM CIS nesou.
- **Opraveno:** `PcCard::Sram` má v paměti atributů CIS: `CISTPL_DEVICE`
  (01h), typ 6 = SRAM, rychlost 150 ns, velikost kódem jednotky
  (512 B × 4^k, nejvýš 32 jednotek), pak `CISTPL_END`. Se zamčenou kartou
  od zapnutí správce souborů v testu vypíše `C:\` a `hryaria`.
- `boot_test` teď vkládá první kartu zamčenou od zapnutí a ptá se na volné
  místo na C: (`INT 21h` 36h); bez CIS to selže, s ním projde. Pak kartu
  odemkne.
- **Otevřené:** když se v tom místě místo 36h soubor otevřel a přečetl,
  na 6.87 (ne 7.05) zůstalo pozdější hlášení baterie úplně tiché, s CIS
  i bez něj. Příčina neznámá; zda jde o chybu emulátoru, nerozhodnuto
  (`ar-z2e`).

**Flash karta: čipy a příkazy, 10. 10. 2026 (`ar-ibv`, krok 1, jen
rozbor).** Rozbalené programy 7.05; tabulky z dat souborů (offsety
v souboru), kód z `tools/dis.py`.

- **Tabulka čipů `DLFFSA.EXE`** (soubor +4CD6h, za běhu DS:0116h, záznamy
  po 20 B, konec FFFFh; čte ji `DLFFSA.EXE+04C2h`): slova výrobce, typ,
  blok mazání v KB na pár, velikost páru v KB, dvě časové konstanty,
  algoritmus, ?, ukazatel zápisu, ukazatel mazání. Čipy jsou vždy
  **v páru**, sudý bajt jeden čip, lichý druhý (slovo `9090h` = příkaz
  oběma).
  - Intel 28F010 (89h B4h) 256 KB, 28F020 (89h BDh) 512 KB, Catalyst
    28F010 (31h B4h), AMD 28F010/020 (01h A2h, 01h 29h): mazání celého
    čipu, algoritmus 1.
  - **Intel 28F008SA (89h A2h): pár 2 MB, blok 128 KB, algoritmus 0**;
    Intel 28F016SA (89h A0h): pár 4 MB.
  - AMD 29F040 (01h A4h), 29F016 (01h ADh), Fujitsu (04h A4h, D5h, ADh):
    algoritmus 2 (AMD, reset `F0F0h`).
- `TRICKSS2.EXE` (soubor +5120h) a `FORMAT.EXE` (rozbalený +7A78h) mají
  vlastní, kratší tabulku: 28F010, 28F020, **28F008SA**, Catalyst, AMD
  28F010/020, AMD 29F010 (01h 20h) a 29F040 — **28F016SA ani 29F016
  neznají**. Čip, který znají všechny tři programy a vejde se do 4 MB
  starého formátu: **Intel 28F008SA** (karty Intel Series 2 byly z nich,
  2, 4, 10 a 20 MB — **[z paměti, neověřeno]**).
- **Rozpoznání** (`DLFFSA.EXE+04C2h`): příkaz `9090h` na adresu 0 karty,
  bajt výrobce na offsetu 0, typ na offsetu 2 (adresa 1 sudého čipu),
  pak `F0F0h`. Velikost karty: smyčka od `+0680h` čte ID na násobcích
  velikosti páru, dokud souhlasí — karta se tedy **nesmí zrcadlit** jako
  SRAM, za posledním párem musí číst něco jiného (FFh).
- **Zápis 28F008SA** (`DLFFSA.EXE+1382h`): `4040h`, datové slovo, čte
  stav, dokud `AND 8080h` = `8080h` (SR.7 obou čipů), `TEST 2020h` ≠ 0 =
  chyba (`5050h` smaže stav), nakonec `FFFFh`.
  **Mazání** (`+140Fh`): `00FFh`, `2020h`, `D0D0h` na adresu bloku, čeká
  na `8080h`, chyba `2020h`, `5050h`, `00FFh`, `FFFFh`. Kolem zápisu
  zapíná Vpp (`0702h` bit 0, `+0D58h`/`+0D84h`, výše).
- 80C188 má 8bitovou sběrnici, slovo přijde jako dva bajty, nižší
  první — emulace tedy může vést každý čip zvlášť (stav, registr SR),
  sudé adresy jeden, liché druhý.

**Návrh emulace (k odsouhlasení):** `PcCard` druhého druhu, flash
z párů 28F008SA (ID 89h A2h, bloky 64 KB na čip = 128 KB karty), každý
čip se stavem čtení pole / ID / stavu, příkazy FFh, 90h, 70h, 50h,
40h/10h (programování jen nuluje bity), 20h+D0h (blok na FFh); hotovo
hned (SR.7 = 1), bez Vpp chyba SR.3. Za posledním párem čte FFh, bez
zrcadlení. CIS s typem flash (5). Obraz = běžná paměť karty bajt po
bajtu; velikost násobek 2 MB, takže 2 MB a 4 MB se s obrazem SRAM
neodliší — druh karty musí nést přípona nebo hlavička. Vyzkouší se
`boot_test`: formát z Arie (`FORMAT.EXE`), zápis a čtení přes DLFFSA,
pak totéž se starou podporou přes TRICKSS2 (E:).
Odsouhlaseno uživatelem 10. 10. 2026: páry 28F008SA, velikosti 2, 4, 10
a 20 MB, obraz s příponou `.flash`.

**Flash karta v emulátoru, 10. 10. 2026 (`ar-ibv`, krok 2).**
`PcCard::Flash` (`src/pc_card.*`): páry 28F008SA podle návrhu výše,
každý čip se svým režimem a registrem SR; Vpp z `0702h` bit 0 jde do
`PcCard::Write`.

- **CIS musí mít i `CISTPL_JEDEC_C` (18h).** Bez něj DLFFSA kartu
  nepřijal: rozpoznání (`DLFFSA.EXE+0B3Dh`) porovná 32 B paměti atributů
  s 32 B společné, a když se liší a začínají n-ticí 01h, rozebere CIS
  (`+089Eh`); ten vyžaduje n-tici 01h **i** 18h, jinak vrátí 0, typ
  zařízení 5 = flash, 6/7 = SRAM, a čip hledá podle kódů z n-tice 18h
  (`+0981h`). Bez 18h DLFMT hlásil „Error: No card in drive - 2H“.
  Karta SRAM n-tici 18h nemá a projde zkušebním zápisem (`+0ADDh`).
- DLFFSA při zkoumání posílá i příkazy AMD (AAh na AAAAh, 55h na 5554h,
  90h, F0h; `+167Ah`); 28F008SA je bere jako čtení pole, jako skutečný.
- **`boot_test`:** prázdná karta 4 MB, `DLFMT.EXE C: /C` přes `INT 21h`
  4B00h (blok paměti pro EXEC alokovaný `48h`), 22 820 změněných bajtů,
  pak C: hlásí **3 792 KB volných**; soubor 4 KB zapsaný, karta vysunutá
  a vrácená, soubor přečtený beze změny. 7.05 i 6.87. DLFMT píše přes
  `INT 21h` AH=06h, teletype log ho nevidí.
- `card_selftest`: ID jen na sudém čipu, programování jen nuluje bity,
  SR po programování, bez Vpp SR.3+SR.4, mazání bloku jednoho čipu (každý
  druhý bajt), chyba posloupnosti 20h bez D0h, druhý pár zvlášť, za kartou
  FFh bez zrcadlení, zámek.
- **Neověřeno:** formát z Arie (`FORMAT.EXE` přes správce souborů, „fleš
  1/2“), starý formát přes TRICKSS2 (E:), karta zasunutá už při zapnutí.
- Další krok: nabídka a obrazy `.flash` (nová karta flash, vložení
  obrazu), pak zkouška uživatele.

**Fleš karta v nabídce, 10. 10. 2026 (`ar-ibv`, krok 3).** Druh obrazu
nese jen přípona: `.flash` = fleš karta (`card_image::IsFlash`, velikost
násobek 2 MB do 64 MB), jinak SRAM. Nová karta má čtvrtý druh „Fleš karta,
nenaformátovaná“ s velikostmi 2, 4, 10, 20 MB (předvybrané 4 MB; seznam
se mění s druhem, velikost zůstane, je-li v novém seznamu); obraz FFh.
Dialog otevření ukazuje `*.img;*.flash`. Titulek říká „fleš karta“.
Naformátovaný fleš obraz emulátor nedělá: FTL zakládá DLFFSA.
`card_selftest`: přípona, velikosti, obraz 10 MB, odmítnutý 1 MB.
Zbývá ověřit u uživatele: formát ze správce souborů, karta ve slotu už
při zapnutí, starý formát přes TRICKSS2 (E:).

**Zkouška uživatele, 11. 10. 2026:** nová fleš karta, formát ze správce
souborů, kopírování souborů — vše funguje, se zvuky, které uživatel znal
ze skutečné Arie (formát i kopírování trvají dlouho; to dělá Aria, ne
karta). E: (stará podpora zapnutá) hlásí „celkové selhání“: karta byla
naformátovaná na C:, tedy FTL, a TRICKSS2 čte jen starý Flash-FAT —
**očekávané**, ne chyba.

- **Jak vznikne starý formát** (`FORMAT.EXE`, rozbalený): FORMAT se ptá
  na písmeno jednotky a každou popíše podle tabulky `[0F3Ah]`
  (`+0BE3h`); jednotka TRICKSS2 se ohlásí zprávou D96h „p c karta, starý
  formát fleš karty“ (`+0C2Ah`). Starý formát je tedy formát jednotky
  **E:**, nový jednotky C:.
- **Typy karty** (`+2954h`, skok `+0655h`): 0 SRAM (zkušební zápis drží,
  zpráva 668h „esram“), 1 chráněná/ROM (666h/66Bh), 2 flash podle ID
  bez CIS (`+274Ch`, 669h „fleš 1“), 4 karta s CIS (66Ah „fleš 2“; naše
  fleš karta, velikost z CISTPL_DEVICE). Pro typ 2 a 4 nad 4 MB řekne
  D98h „nemůže formátovat více než čtyři megabajty se starým formátem“
  a nabídne D99h „použít jenom čtyři megabajty“ (`+0600h`–`+0638h`).
- Starý formát v emulátoru zatím neověřen (čeká na zkoušku: formát E:).
  **Ověřeno uživatelem týž den:** formát jednotky E: a práce s ní fungují
  (TRICKSS2, Flash-FAT, příkazy 28F008SA z jeho vlastní tabulky).

### 6.9 Napájení

`TURNOFF.COM` (16 B): `cli; mov dx,0701h; mov al,0; out dx,al; jmp $` —
port `0701h` vypíná. Stav baterie zatím neznámý.

**API.DOC (6. 10. 2026):** standby po 5 s nečinnosti (nastavitelné),
v něm **běží CPU na 1/16 taktu**; napájení sériového portu se zapíná
a vypíná příkazem (`POWER_SERIAL_ON/OFF`); `K_SLEEPY` dostane program
před vypnutím. Funkce `Power` mají 20 příkazů (`ARIA-INT.H`). Starší
nízkoúrovňové API v `ARIACONS.PAS` (INT 60h, dnes v C hlavičce
nahrazené): `AAPI_SET_VPP` 0100h, `ENABLE_CARD_VCC` 0101h,
`ENABLE_RS232` 0102h, `RESET_RS232` 0103h, `GET_BATTERY_STATUS` 0104h,
`ENABLE_CARD` / `ENABLE_FLASH` / **`ENABLE_RAM`** 0105h–0107h,
`DISABLE_WINDOW` 0108h, `SET/GET_CARD_ADDR`, `SET/GET_FLASH_ADDR`.
Pojmenovává tedy přesně hardware z 6.2 a 6.8 a říká, že okno umí
ukázat i **RAM** — režim 1 latche `0580h`, který jsme v kódu neviděli,
je nejspíš RAM **[předpoklad]**.

**Požadavek uživatele, 7. 10. 2026: RAM drží, dokud drží baterie.** Paměť
Arie, včetně RAM disku D: (319 KB na `B0000h`–`FFBFFh`, 6.2) a paměti
CMOS hodin (6.5), napájí baterie; reset a vypnutí ji nesmažou. Má to
fungovat jako v Eurece: **teplý reset** (na Arii tlačítko zapnutí/resetu
na levém boku, popis z roku 1997, 2) paměť ponechá, **úplný reset**
odpovídá odpojení baterie a paměť smaže, a stav se při řádném vypnutí
uloží, takže další spuštění emulátoru naváže. Zatím neplatí:
`AriaMachine::PowerOn()` při každém startu nuluje celou RAM a CMOS má
výchozí hodnoty. Bead `ar-ram`. Pozor na pořadí startu: RAMDISK.EXE
(Datalight VDISK „static“) disk při zavedení najde, nebo založí; zda
obsah po teplém resetu převezme, ukáže až běh.

**8. 10. 2026, stroj:** `AriaMachine::PowerOn()` už RAM nenuluje a
CMOS hodin nechává (CMOS se nenulovala ani dřív, jen se `PowerOn()`
nevolalo podruhé); nulování a výchozí CMOS dělá nové `BatteryCut()`.
**Ověřeno během:** `boot_test` po prvním startu zapíše do kořenového
adresáře D: (`B0600h` podle BPB VDISK na `B0000h`) položku, zapne stroj
znovu s RAM a po druhém startu (7.05 i 6.87) ji tam najde — RAMDISK.EXE
disk převezme. Zkouška opačně: po `BatteryCut()` disk založí znovu a
položka zmizí. Podpis „VDISK“ v zaváděcím sektoru to **nerozhoduje**:
s přepsaným podpisem ho VDISK obnoví a adresář nechá; podle čeho disk
pozná, zatím nečteno. `INT 19h` volají jen nástroje karet (TRICKSS2,
CARDMAN, DLFMT, FORMAT, `tools/ints.py`), start ne.

Zbývá: okno zatím volá `PowerOn()` jen jednou, takže uživatel změnu
neuslyší. Chybí zapnutí po vypnutí (`0701h`), restart po `INT 19h`,
klávesy pro teplý reset a odpojení baterie a uložení paměti do souboru
mezi spuštěními emulátoru.

**Doplněno 8. 10. 2026, okno (krok 2):** všechno z předchozího odstavce
je hotové. Stroj → Reset (odpojit baterii, s dotazem), Teplý reset,
Vypnout Arii (`F11`, `Ctrl+R`/`P`/`V`, jako Eureka); vypnutí (vlastní
`0701h` nebo z hostitele) a zapnutí ohlásí okno tóny Eureky a titulkem,
vypnutá Aria pustí klávesnici. `INT 19h` stroj znovu zapne s RAM.
Paměť (RAM 1 MB + celé RTC s posunem hodin, `AriaMachine::SaveState`)
se ukládá při zavření okna, jen když je Aria vypnutá nebo ji uživatel
v dotazu vypne (rozhodnutí uživatele: dotaz na Alt+F4); soubor
`pamet-<CRC-32 obrazu>.bin` (`src/snapshot.*`) v `config` vedle EXE
nebo `%APPDATA%\Aria`, při startu přečten a smazán. Jeden soubor na
sestavení, takže 7.05 a 6.87 nemají společnou paměť a dotaz „jiná ROM“
z Eureky odpadá. `boot_test` druhý start dělá z nového stroje načteného
ze souboru.

**Otevřené: „konec“ nevypíná.** Podle uživatele Arii vypíná akord všech
čtyř šipek (API: `K_UDLR` = 18Fh, `ARIA-API.H`). V `boot_test` po něm
MAINMENU promluví (asi 12 000 bajtů do DSP), ale do 20 s nezapíše
`0701h`; CPU stojí na `355C:01E0`, ne ve vypínací smyčce AAPI. Ta
(`AAPI.EXE+DDF0`, `POWER_SWITCH_OFF`) čeká, až `INT 61h` AX=010Ah
(SpeechSpeaking, `SPEAK.EXE+8512h`: příznak `[123Eh]` nebo neprázdný
kruh textu `[1B6Eh]`/`[1B70h]`) vrátí 0, pak 100 ms (`+DE0C`, zpoždění
v ms, 6.4) a `out 0701h,0`. Nejspíš MAINMENU po „konec“ něco říká a čeká
na odpověď **[předpoklad]**; co řekl, ukáže až záznam textu z
`INT 61h`.

**Neplatí (uživatel, 9. 10. 2026):** MAINMENU se na nic neptá, po čtyřech
šipkách jen řekne „vypnuto“ a Aria se vypne. Podezření se tím přesouvá na
čekání AAPI na konec řeči: SpeechSpeaking v emulátoru nejspíš nikdy
nevrátí 0 **[předpoklad, k ověření]**.

**Ověřeno 9. 10. 2026, neplatí ani to.** Pokus v `boot_test` (dočasný,
nezapsaný): vektor `61h` přes pahýl v ROM, který volání zaznamená a
skočí dál do SPEAK (`0Fh 61h; jmp far`), pak akord čtyř šipek a krokování
3 s. Zjištěno:

- Po akordu přijde `INT 61h` AX=0101h (ztišení, BKB při stisku), pak
  text `!<TO14/100/10000/1>` = **cvaknutí klávesy BKB** (`IDS_KBD_KLIK5`
  0C16h v ARIAMSG.H, MESSAGES.DAT `+9709h`), jinak žádný text.
- **MAINMENU svůj případ `K_UDLR` nedostane:** `MAINMENU.EXE+087Ah`
  porovná klávesu s `18Fh` a skočí na `+09AFh` (volání s 8, nejspíš
  zvuk, pak `Power(0, POWER_SWITCH_OFF = 2)`, ARIA-INT.H); kód na `+09AFh`
  se během 3 s nevykoná. `Power` v AAPI (tabulka skoků `CS:0383h` při
  základu segmentu `DC40h`) na příkaz 2 volá rovnou `+DDF0h`, ani ta se
  nevykoná.
- SPEAK je potom v klidu (`[123Eh]` = 0, kruh textu prázdný), tisíce
  dotazů AX=010Ah jsou obyčejná nečinná smyčka MAINMENU (byly i před
  akordem). Čekání na řeč tedy vypnutí neblokuje.

Klávesa se tedy ztratí nebo změní mezi frontou BKB (`+0663h`, kód `8Fh`
z `+0437h`–`+047Ah`) a MAINMENU. Další krok: zjistit, jaký kód MAINMENU
přes AAPI (`GetKey`, `INT 16h` BKB `+0DC5h`) po akordu skutečně dostane.

**Vyřešeno 9. 10. 2026 — emulátor je v pořádku, test ne.** Obalem
`INT 16h` (záznam AH a vráceného AX): AAPI čte klávesy `INT 16h` AH=21h
z `3246:0086` a akord **dostane jako `018Fh`** (šipka nahoru `0181h`,
F1 `01C0h`). Na zásobníku byl text „, ano nebo ne?“: v testu po
studeném startu visí otázka MAINMENU **„ram disk může být poškozen,
formátovat, ano nebo ne?“** (`IDS_MM_RAMDISK_CORRUPT` 114h). MAINMENU
před ní porovná 8 bajtů na `B000:0030` (zaváděcí sektor RAM disku) se
vzorem z `DS:011Ch` (`MAINMENU.EXE+0729h`–`+0796h`, porovnání `+3B9Bh`);
po formátování (`+0678h`) vzor zapíše, takže otázka přijde jen po
prázdné paměti. Otázka bere všechny klávesy, proto akord konec i F1 v
testu „nic nedělaly“. Ano je **`y`** i v české verzi (MESSAGES.DAT
„yYnN“, `IDS_YES_CHARS` 83h); `a` otázku jen zopakuje. Po `y`:
„ano“, „formátování skončeno ok“, pak konec → MAINMENU dostane `18Fh`,
řekne zprávu 8 **„vypnuto“** (MESSAGES.DAT: 0 selhání, 3 ano, 5 „, ano
nebo ne?“, 8 vypnuto) a AAPI zapíše `0701h` — přesně podle uživatele.
**To vysvětluje i formátování „při každém zapnutí“:** emulátor dřív
začínal s prázdnou pamětí. `boot_test` to teď ověřuje celé: otázka po
studeném startu, `y` naformátuje, konec řekne „vypnuto“ a vypne,
druhý start ze souboru paměti otázku nepoloží a položka na D: zůstane
(7.05 i 6.87). Řeč zaznamenává `SpeechLog` v testu (pahýl na
`FFC0:0240h`, `INT 61h` AX=0201h řetězec v ES:DI, AX=0200h znak v BL).
Bead `ar-rp0` uzavřen.
Záznam `INT 61h` pahýlem stojí za to udělat trvalým (`boot_test -v`).
`TURNOFF.COM` (uživatel si na něj vzpomněl) je na B: a dělá jen
`cli; out 0701h,0` — totéž co konec `+DDF0h`.

### 6.10 Rozhodnutí: vrstva DOS

Buď jádro FreeDOS na HLE BIOSu, nebo `INT 21h` v hostiteli. Riziko: ovladače
(`DLFFSA`, `BACKPACK`, `BPHDARIA`) a `COMMAND.COM` z ROM-DOS mohou záviset
na vnitřnostech ROM-DOS (List of Lists, DPB, řetěz ovladačů). Rozhodne
fáze 2.

**Rozhodnuto uživatelem 6. 10. 2026: jádro FreeDOS.** Důvody: CardTrick
(„Increase LASTDRIVE“) a ovladače Datalight sahají do vnitřních struktur
DOSu, takže je potřeba jádro vnitřně kompatibilní s MS-DOS, ne jen
`INT 21h`; FreeDOS takové struktury napodobuje. Ověřeno 6. 10. 2026 na
GitHubu (FDOS/kernel): licence **GPL v2 nebo novější**, aktivní vývoj,
vydání **ke2046** (30. 6. 2026) má sestavení **pro 8086**
(`ke2046_86f16.zip`, FAT12/16), takže na 80188 nepotřebuje instrukce
286. Jádro se nebude kompilovat do EXE: poleží vedle emulátoru jako
samostatný soubor (agregace, licence MIT zůstává čistá), podobně jako
doplněk NVDA pod GPL. Shell: nejdřív `COMMAND.COM` ROM-DOS z B: (tak
startuje CONFIG.SYS), FreeCOM jako záloha, kdyby na FreeDOSu nešel.

Otázky k ověření až za běhu: zda ovladače ROM-DOS-specifické
(`VOICE.SYS` hledá `FLASHDSK.SYS` na pevných segmentech, 6.1) na FreeDOSu
jen ohlásí, že záplata nenašla cíl (to umí), a zda `COMMAND.COM` ROM-DOS
(1993) přijme verzi DOSu, kterou FreeDOS hlásí.

**První start, 6. 10. 2026 (`boot_test`, CC0705, ke2046 pro 8086).**
BIOS emulátoru (`src/hle_bios.cpp`) nahradí zavaděč: `KERNEL.SYS` vloží
celý na `0060:0000` a skočí na něj s jednotkou startu v `BL` = 1 (B:),
jak to dělá `boot.asm` FreeDOSu. Jádro je zabalené UPX; rozbalí se samo
(rozbalovač běží na `12E5h`, výstup sahá do `12E1Ch`), vypíše stopovací
znaky „123“ a úvodní text verze 2046, pak „no hard disks detected“, „No
supported partitions found.“ a **„Bad or missing Command Interpreter:
command.com /P /E:256“**. Výchozí shell znamená, že `CONFIG.SYS` z B:
nepřečetlo (tam je `shell=b:\command.com`); proč, je další krok.

Poučení z prvního pokusu: zaváděcí sektor ani zásobník nesmí ležet na
`0000:7C00`. Jádro (zabalené `0600h`–`BEDAh`) to místo překrývá a jeho
vstup ukládá `BP` na zásobník volajícího; kopie sektoru tam prohnala 512
bajtů komprimovaných dat a UPX pak rozbalil díru přesně přes
`INIT_TEXT` (`026B:9EA0`, podle `kwc8616.map`). Zásobník je proto na
`0000:0600` a `BP` = 0 (podle `kernel.asm` „žádný příkazový řádek“).

**Druhý start, týž den: CONFIG.SYS i AUTOEXEC.BAT z B: běží.** Výchozí
shell z prvního startu už neplatí; měl dvě příčiny:

1. Jádro odmítlo zaváděcí záznam B:, protože nemá podpis `55AAh` (6.11),
   a počítalo s disketou 1,44 MB. Poznalo se to podle záznamu volání
   `INT 13h` (`boot_test -v`): kořenový adresář četlo na hlavě 1,
   sektorech 2–15, což je přesně adresář 224 položek za dvěma FAT po
   9 sektorech.
2. Tik `INT 08h` vůbec neběžel (chyba hodin CPU, 6.12), takže čekání na
   F5/F8 („Press F8 to trace or F5 to skip“) neskončilo.

Po obou opravách proběhne celé CONFIG.SYS: `VOICE.SYS` 1.3 (ohlásí
„Could not find FLASHSK.SYS to patch.“ a pokračuje, jak 6.1
předpovídala), CardTrick 3.01 s jednotkou C:, Datalight VDISK 5.0 jako
**D: s 319 KB** (shoduje se se vzpomínkou uživatele), `BACKPACK.SYS`
a `BPHDDRV.SYS`. **`COMMAND.COM` ROM-DOS na FreeDOSu běží** a vykoná
AUTOEXEC: `trickss2` (karta E:), `aapi -c` („Aria API v1.1b“), `speak
-i` („Aria version Cze2/3.0.0 is now installed.“), `bkb` (český ovladač
1.10, „Segment = 58E1“), `fossil`, `ariamode` a `arialarm`. Za
`arialarm` se běh zastaví (zatím nezkoumáno). Během toho SPEAK 886×
přistoupil na port `0401h` (DSP, 6.3) a programy sáhly na neobsloužené
porty `0402h`, `0581h`, `0680h` a `0682h`. `boot_test` od té doby
kontroluje šest milníků až po BKB; celý start trvá asi sekundu.

**Za `arialarm`, 7. 10. 2026.** Zastavení mělo dvě vrstvy. První: tik
stál, protože AAPI přeprogramoval časovač 2, kterým BIOS dělil tik
(6.1); po přechodu na samotný časovač 0 ARIALARM vypíše „Aria Alarm
Scheduler v1.08“. Druhá: ARIALARM v háku `INT 08h` (`67A9:0345`, segment
ARIALARM) zavolá předchozí obsluhu, a když DOS není zaneprázdněný a jeho
příznak `[cs:44h]` je nastavený, spustí přímo z přerušení svou rutinu
`67A9:0217`. Ta chce mluvit a SPEAK pak v `4732:62A0` čeká na DSP, které
neodpoví (6.3). Stroj tak stojí uvnitř přerušení časovače (řadič hlásí
in-service bit 0). Proč ARIALARM po startu mluví, není zjištěné. Další
krok: jímka DSP podle 6.3, aby SPEAK neuvázl.

**S jímkou DSP (týž den) AUTOEXEC doběhne až do `mainmenu`:** za
ARIALARM následuje `rundos` („Aria RunDOS Utility v1.1“), `prompt
$p$g`, `path=b:\system;b:\dos`, `atkbd.com` (externí klávesnice
„Disabled.“, protože CMOS `57Eh` nemá „C“) a `mainmenu`. Totéž platí pro
7.04 anglicky, 6.99 a **6.87 v Kamenických** (AAPI 1.1a, řeč
„Cze1/3.0.0“, ARIALARM 1.07). `boot_test` běží v sadě na 7.05 i 6.87
a kontroluje devět milníků až po `mainmenu`. Zda MAINMENU čeká na
klávesu a co řekl, je další krok (záznam textu z `INT 61h`).

### 6.11 Rozhodnutí: vrstva flash

Buď skutečná emulace flash čipu a jeho okna (nutné, pokud zápis `DLFFSA`
jde přes `0702h`/`0480h`), nebo disk B: jako HLE blokové zařízení.
Rozhodne fáze 2.

**Stav po fázi 2 (6. 10. 2026), návrh:** `DLFFSA` je ovladač karty
(C:), ne flash, takže otázka se týká jen B:. Rozumné rozdělení:

- **Okno D000h emulovat skutečně** (latche `0580h`/`0480h`, režim 2 =
  stránka obrazu B:, 6.2): čtou z něj AAPI a SPEAK přímo, mimo DOS.
- **Disk B: obslouží náš vlastní malý ovladač** místo chybějícího
  `FLASHDSK.SYS`: blokové zařízení pro DOS, které čte sektory z obrazu
  (oknem, nebo pastí do hostitele). Zápis na B: (aktualizace, nastavení)
  půjde do překryvného souboru hostitele, ne do obrazu firmwaru.
- Programování flash čipu (příkazy čipu, Vpp) emulovat netřeba, dokud ho
  nevolá kód z B:; dosavadní rozbor takový kód nenašel.

**Upřesnění 6. 10. 2026:** „vlastní ovladač“ pro B: je nakonec
`INT 13h` BIOSu emulátoru, ne ovladač v CONFIG.SYS. BIOS hlásí dvě
disketové jednotky (slovo vybavení `0261h`): 0 = A:, malý prázdný
svazek FAT12 místo ztraceného ROM disku (FreeDOS by jinak z B: udělal
fantom A:), 1 = B: z obrazu. Zaváděcí záznam B: (`DLRDISK`) udává 61 440
sektorů na stopu a 1 hlavu na 2 465 sektorech, což se do 6bitového pole
sektoru v `INT 13h` nevejde. BIOS proto počítá s 63 sektory a 16
hlavami a zaváděcí sektor, který přes `INT 13h` vydá, v těch dvou
polích opraví; obraz sám se nemění. Zápisy jdou do kopie obrazu
v paměti (překryvný soubor zatím není). Okno D000h je emulované podle
6.2 (režim 2 = stránka obrazu, 3 = karta zatím `FFh`, 1 a 0 = RAM).

**Podpis zaváděcího záznamu (6. 10. 2026):** `DLRDISK` nezapisuje
`55AAh` na `1FEh`; v 7.05 tam je `59h 1Fh`, v 7.04 `47h 63h` a
`24h`–`3Fh` je `FFh`, tedy nenaprogramovaná flash. `FLASHDSK.SYS`
z ROM-DOS na podpis zjevně nehleděl, FreeDOS (`dsk.c`, `getbpb`) bez
něj BPB zahodí. BIOS proto do zaváděcího sektoru, který vydá, kromě
geometrie doplní i podpis.

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

**Rozbalení PKLITE, 6. 10. 2026.** `tests/pklite_unpack.cpp` spustí stub
programu na jádru s malým DOSem v hostiteli (`INT 21h` 09h, 25h, 30h,
35h, 4Ah; vše ostatní je chyba). Všech 17 programů PKLITE 1.14 na B:
(4 s „extra“ kompresí) má stejný tvar: vstup `FFF0:0100`, stub se
přesune výš (při velké minimální alokaci *dovnitř* paměti programu, takže
konec nejde poznat podle adresy) a skočí daleko na původní vstup; konec
běhu je proto **druhá změna CS**. Tabulka relokací se získá dvěma běhy
s PSP `1000h` a `2000h`: relokované slovo se liší o `1000h` a projeví se
jen na vyšším bajtu. Kontrola: relokace > 0, žádný text stubu, autorské
právo runtime (Borland, u `DLFMT.EXE` Datalight). Projde pro 7.05, 6.87,
6.99, 7.04 i 6.90; `check-unpack` v testech (7.05). Mutace (konec na
první změně CS) shodí všech 17. Výstup jen pro analýzu a jen mimo strom
(nástroj odmítne cíl uvnitř repozitáře), např. `D:\aria\unpacked\705`.
Cesty UNC z WSL: `fs::weakly_canonical` hází a `fs::absolute` ztratí
zpětné lomítko, proto `GetFullPathNameW`.

**Otevřené k 6.12:** kmitočet CPU Arie není známý (potřebný pro převod
taktů na reálný čas a pro časovače), patří k 6.5 a fázi 3.

**Chyba hodin mezi běhy (nalezena a opravena 6. 10. 2026).** MAME
`m_icount` neinicializuje (jeho plánovač ho nastaví před každým
úsekem). Naše hodiny taktů ho ale čtou i mimo běh (`UpdateClock`:
základ + (`m_icount_base` − `m_icount`)), takže časovač naprogramovaný
před prvním během se naplánoval proti smetí: ve stroji na haldě hodiny
po zapnutí ukazovaly 1 031 281 408 taktů, a když první běh hodiny
srovnal, časovač 2 čekal miliardu taktů. Tik BIOSu proto nikdy
nepřišel. `cpu_selftest` dřív nic neodhalil, protože jeho CPU na
zásobníku mělo v těch místech náhodou nuly. Oprava je ve fasádě
(konstruktor `Cpu80188` oba čítače vynuluje), kód MAME zůstal
nedotčený. Test `TestPrescaledTimer` staví CPU do paměti vyplněné
`A5h` a programuje časovače přes `OutWord` mimo běh, před dlouhým
`REP MOVSW` jako ve stubu UPX; bez opravy selže (mutace ověřena), s ní
projde. Týž test ověřuje i předdělič (bit P, časovač 0 počítá konce
časovače 2), který tik BIOSu používá.

### 6.13 Programy Arie na PC (DOS, DOSBox-X)

Uživatel by rád programy Arie spouštěl i bez emulátoru, v DOSu nebo
DOSBox-X. Rozbor PC verze `AAPI.EXE` z techmanu (6. 10. 2026):

- „Aria API v1.1b“, (c) 1994–1999, ladicí sestavení Borland C++ (aserce,
  ladicí `printf`), **nekomprimované**, 1 381 relokací. DGROUP na odstavci
  `11AEh` (vstup `PCAAPI+0000h`).
- **Není bez hardwaru.** Stejně jako verze z Arie volá `INT 69h` ×6 (okno
  flash: kopie `+0A955h`, procházení adresáře `+0AA76h`), `INT 61h` jen
  ×2 (funkce `0401h`, uživatelský slovník), `INT 64h` ×1 (služba BKB) a
  sahá na porty RTC (`0501h`–`0505h`, `050Ch`), `0580h`, `0680h`, `0702h`.
  Na rozdíl od verze z Arie nečte klávesnici `0700h` a nevypíná `0701h`.
- **Start řeči** (`+0AB80h`): když `INT 61h` chybí, jen vypíše „Warning:
  Speech not loaded“, ale pak **bezpodmínečně** volá hledání `PT.DAT`
  v adresáři flash (`+0AAD7h`) přes `INT 69h` a instaluje `INT 62h`. Na
  PC s obyčejným MS-DOSem je vektor 69h většinou `0000:0000`, takže se
  vykoná tabulka vektorů jako kód — pád nebo zamrznutí hned při startu;
  to by vysvětlovalo, proč uživateli AAPI na PC nikdy pořádně nefungovalo.
  DOSBox a DOSBox-X mají nepoužité vektory nasměrované na IRET, tam by se
  volání jen vrátilo a hledání nic nenašlo **[předpoklad, ověřit]**.
- **Řeč na PC = řádek textu.** `+0F2F8h` píše mluvené znaky přes `INT 10h`
  na **řádek 21** obrazovky s předponou `Speak "`, sloupec v `[1130h]`
  zalamuje na 78. Je to vždy zapnuté, žádný přepínač.

**Klávesnice na PC:** PC verze AAPI volá `INT 16h` jen se standardními
funkcemi BIOSu (00h, 02h, 10h, 11h), kdežto verze z Arie rozšíření BKB
(21h–30h). Robotron tedy klávesnici pro PC záměrně přestavěl na BIOS.
Aplikace volají rozšíření přímo jen málo: 2Eh (ARIALARM, MAINMENU, ZM),
2Fh (ARIAMODE), 30h (MAINMENU); AWP, FILEMGR, COMMS jdou přes AAPI.
Na PC tedy stačí malá náhrada `INT 16h` pro 2Eh, 2Fh, 30h.

Co by programy Arie na PC potřebovaly navíc (rozbor, nic z toho není
napsané): náhradu `INT 69h` (stub, nebo malý TSR, který stránky obrazu
flash kopíruje do okna D000h), náhradu `INT 61h` (text místo hlasu, nebo
předání ven), `INT 64h` a `INT 63h` (klávesnice BKB) a ošetření přímých
přístupů aplikací na porty (`MAINMENU.EXE` čte `0480h` a `0700h`; na PC
čtou `FFh`). Funkce `INT 63h`/`64h` a jejich volající v aplikacích jsou
zatím neznámé (6.1, 6.4).

### 6.14 Technický manuál — co z něj ještě je

Prošlo 6. 10. 2026: `ARIA-INT.H` (API, Speech, **MODE 63h**, **Alarm
65h**, Power, Printer), `ARIA-API.H` (prototypy, struktury, kódy kláves,
`GetAriaVersion` vrací verzi flash, sestavení, **verzi boot ROM a sériové
číslo**), `ARIAKBD.PAS`, `ARIACONS.PAS` (starší nízkoúrovňové API, 6.9),
`ARIA.C` (vazba na AAPI **podle pořadových čísel** přes `INT 60h AX=0100h`;
pořadí se nesmí měnit — náhrada AAPI na PC by je musela dodržet),
`ARIAMSG.H` (čísla hlášek; nastavení MODE končí `IDS_MODE_OLD_PCCARD` a
`IDS_MODE_IBMKB`), API.DOC (popisy funkcí, kódy kláves s akordy, standby,
formáty souborů; sekce „Interrupts“ je jen prázdný nadpis).

Výklad CMOS z 6.5 podle hlášek MODE: `057Eh = „C“` ↔ **IBM klávesnice**
zapnutá (ATKBD je ovladač vnější klávesnice IBM/AT přes `INT 14h`, bez
„C“ skončí), `057Fh = „R“` ↔ nová podpora karet místo „old PC card“
(TRICKSS2 bez „R“ odmítne start) **[předpoklad, slabší]**.

Zbytek (`ARIA.PAS`, `SPELL.PAS`, `DEMO.C`, `GLOBAL.C`, `MAINMENU.C`,
`MSG.C`, `YESNO.C`, `MESSAGES.*`, `MAKEMSG.EXE`) je ukázkový program a
vazby pro Pascal; o hardwaru nic. `SPEECH.DOC` bude potřeba až pro
náhradu `INT 61h` na PC (řídicí sekvence `!<..>` odfiltrovat). **Manuál
je tím pro hardware vyčerpaný**; dál slouží jako reference API.

## 8. Pořadí

1. Fáze 0: kostra repozitáře — **hotová** 5. 10. 2026: dokumenty,
   Makefile, skripty pro WSL i dávky `.bat`, `.clangd`, načtení disku B:,
   analytické nástroje v `tools/` a beads.
2. Fáze 1 **hotová** 6. 10. 2026: jádro 80188 s periferiemi PCB a
   rozbalení PKLITE (6.12), tabulka přerušení s rozbalenými programy (6.1).
3. Fáze 2: 6.1 a 6.2 jako první, protože na nich stojí všechno ostatní.
4. Fáze 3 (`ar-4cu`) začala 6. 10. 2026: jádro FreeDOS nabootuje po úvodní
   text (6.10). Další kroky: proč jádro nečte `CONFIG.SYS` z B:, pak
   `COMMAND.COM` ROM-DOS, ovladače z CONFIG.SYS, AUTOEXEC až po
   `aapi -c` a MAINMENU, se záznamem textu pro řeč z `INT 61h`.
   Stav týž den večer: CONFIG.SYS a AUTOEXEC proběhnou až po `arialarm`
   (6.10); další krok je, proč se běh za `arialarm` zastaví.
   7. 10. 2026: s jímkou DSP (6.3) start doběhne do MAINMENU na 7.05,
   7.04, 6.99 i 6.87. Další: záznam řeči z `INT 61h`, klávesy do BKB.
5. Fáze 4 (`ar-6uh`) začala 7. 10. 2026: skutečný ADSP-2105 ve stroji,
   `LPC.DSP` a `PLAY.DSP` vydávají řeč a harfu (6.3). Otevřené:
   vzorkovací frekvence (poslech), latch `2000h`, bity `0401h`; pak
   výstup zvuku v aplikaci. Týž den: aplikace hraje start živě
   (`EmulatorThread`, 6.3); další krok jsou klávesy do BKB.
