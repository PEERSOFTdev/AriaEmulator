# Mapa hardwaru Arie

U každého údaje je napsáno, čím je doložen:

- **[binárka]** — odvozeno z kódu na disku B: (soubor a místo),
- **[API]** — popsáno v Aria API Reference nebo hlavičkách (`ARIATECH`),
- **[web]** — robotron.net.au/aria.htm,
- **[předpoklad]** — zatím neověřená hypotéza.

Stav k 5. 10. 2026: **předběžné.** Většina EXE je komprimovaná PKLITE a
jejich sken portů je šum; níže je jen to, co pochází z nekomprimovaných
binárek a z instrukcí `mov dx,imm16` těsně před `in`/`out`.

## Procesor a periferie na čipu

- CPU 80C188 **[web]**, klasická varianta, ne EB: používá se DMA kanál 0
  (`SPEAK.EXE+09F0h`), který EB nemá **[binárka]** (HANDOFF 6.12).
- Řídicí blok periferií (PCB) je přemístěn do I/O na adresu `0000h`
  **[binárka]**: registry na offsetech podle Intelu sedí s tím, co
  programy dělají. Emulátor má celý PCB z MAME (HANDOFF 6.12), takže
  níže uvedené porty už fungují; offset v MAME je číslo slova, port =
  základ + 2 × offset.
  - `22h` EOI řadiče přerušení (`FOSSIL.COM`, `SPEAK.EXE`)
  - `34h` řízení přerušení DMA0 a `3Eh` řízení INT3 (`SPEAK.EXE+0F89`,
    `+6794`); DSP tedy nejspíš hlásí rámce přes INT3 a DMA0
    **[předpoklad]**
  - `38h`, `3Ah` řízení INT0, INT1 (`FOSSIL.COM`)
  - `5Ah`, `5Eh` časovač 1 (`FOSSIL.COM`)
  - `A0h` UMCS (`RAMDISK.EXE` čte i zapisuje) **[předpoklad]**
  - `C0h`–`CAh` DMA kanál 0: zdroj, cíl, počet, řízení (`SPEAK.EXE`)
- DSP ADSP-2105 **[web]**; programy `LPC.DSP` a `PLAY.DSP` nahrává
  `SPEAK.EXE` za běhu **[binárka]**.

Pozor: `BACKPACK.SYS` (obecný PC ovladač mechanik Backpack) sahá na porty
časovače PC `42h`, `43h` a reproduktoru `61h` (`BACKPACK.SYS+360Ah`–`364Bh`). Na Arii tam leží PCB, takže emulace musí takové přístupy snést.
Sken portů pochází z `tools/ports.py` (7.05, bez PKLITE programů).

## Paměť

- 1 MB nevolatilní RAM, 2 MB flash **[web]**.
- Disk B: = flash se souborovým systémem FAT12 Datalight **[binárka]**;
  adresa ve flash = offset v obrazu disku (AAPI hledá soubory přímo
  v obrazu, `AAPI.EXE+A92Ch`) **[binárka]**.
- **Okno 64 KB na D0000h–DFFFFh** (segment D000h), stránky po 64 KB,
  vybírá je latch `0580h` (bity 1–0 režim: 0 vypnuto, 2 interní flash,
  3 karta; bity 7–2 stránka) a pro kartu i `0480h` (bity 5–0 vyšší bity
  stránky, bit 7 nejasný) **[binárka]**; správce `INT 69h` ve
  `VOICE.SYS`, HANDOFF 6.2.
- **RAM disk D: na B0000h–FFBFFh (319 KB)** pod oblastí boot ROM: vidět
  jen s UMCS = FFF8h, kdy horní chip select pokrývá jen FFC00h–FFFFFh
  (`RAMDISK.EXE+037Eh`) **[binárka]**.
- Neznámé: normální UMCS (velikost ROM), A0000h–AFFFFh, velikost RAM
  pro DOS.

## Porty desky (předběžně)

Doplněno 6. 10. 2026 s rozbalenými programy PKLITE (`tools/ports.py`
s `ARIA_UNPACKED`, HANDOFF 6.1). Nové porty a uživatelé:

- **`0500h`–`057Fh` — RTC kompatibilní s MC146818/DS12887, registr n na
  `0500h + n`** **[binárka]** (HANDOFF 6.5): alarm sekund/minut/hodin
  `0501h`/`0503h`/`0505h` (`AAPI.EXE+46F0h` ruší alarm „C0h“), registr C
  `050Ch` (bit 5 AF, `ARIALARM.EXE+2132h`), registr D `050Dh` (bit 7 VRT,
  `BKB.COM+1613h`), paměť CMOS s nastavením: `057Ch` (ARIACLK, ARIADY,
  ARIALARM), `057Eh` = „C“ podmínka pro ATKBD, `057Fh` = „R“ podmínka
  pro TRICKSS2 a FORMAT; zapisuje je `ARIAMODE.EXE`
- `0580h` — zapisuje i `AAPI.EXE` (`+48A7`, `+4944`) a `FORMAT.EXE`
- `0680h` — čte `AAPI.EXE+A017`
- `0700h` — čtou i `AAPI.EXE+DCC7` a `MAINMENU.EXE`; `0701h` zapisuje i
  `AAPI.EXE+DE17`; `0702h` zapisují i `AAPI.EXE` a `FORMAT.EXE`
- `0480h` — čtou i `FORMAT.EXE` a `MAINMENU.EXE`, `FORMAT.EXE` i zapisuje.
  **Čtení = stav slotu karty** (bity 0–1 detekce, aktivní nulou; bity 2–3
  nejspíš BVD; bit 5 další stav), **zápis = vyšší bity stránky okna**
  **[binárka]** (HANDOFF 6.2, 6.8)
- `0702h` — napájení slotu: 6 s kartou, 4 bez ní, 7 během zápisu
  (`TRICKSS2.EXE+14EDh`, `+1549h`) **[binárka]**; výklad bitů
  **[předpoklad]** (HANDOFF 6.8)
- `FFFF:000Eh` — bajt, který čte AAPI (`AAPI.EXE+46DEh`); na PC modelový
  bajt BIOSu, na Arii ho musí dát boot ROM (HANDOFF 6.5)

Porty s pevnou adresou `00h`–`FFh` (`in al,imm`) zůstávají ve výpisu
zašuměné daty; za doklad se berou jen porty načtené do `DX`.

- `0401h`, `0402h` — zápis z `SPEAK.EXE`, nejspíš DSP **[předpoklad]**
- `0480h` — čtou `BKB.COM`, `DLFFSA.EXE`, `TRICKSS2.EXE`, zapisuje
  `VOICE.SYS`
- `050Ch`, `050Dh` — čtou `ARIALARM.EXE`, `BKB.COM` (registry C a D RTC,
  viz výše)
- `057Eh`, `057Fh` — čtou `ATKBD.COM`, `TRICKSS2.EXE` (paměť CMOS RTC,
  viz výše)
- `0580h` — latch okna D000h (režim a stránka, HANDOFF 6.2); „sdílený
  s hlasem“ byl dřívější výklad, který kód nepodpořil. Správce `VOICE.SYS` přes
  `INT 69h`; dřív do něj přímo zapisoval `FLASHDSK.SYS` z boot ROM, který
  VOICE.SYS přepisuje (`VOICE.SYS+0204h`) **[binárka]**; zapisuje i
  `RAMDISK.EXE`
- `0581h`, `0584h` — čte `BKB.COM`
- `0600h`, `0601h`, `0681h` — `FOSSIL.COM`, nejspíš UART **[předpoklad]**
- `0700h` — čte `BKB.COM`, nejspíš klávesnice **[předpoklad]**
- `0701h` — zápis 0 vypne stroj, `TURNOFF.COM+0` **[binárka]**
- `0702h` — zapisují `DLFFSA.EXE`, `TRICKSS2.EXE`; dřívější odhad „flash“
  neplatí, je to napájení slotu karty (výše, HANDOFF 6.8)

## Softwarová přerušení

- `60h` Aria API (`AAPI.EXE`) **[API]**
- `61h` řeč (`SPEAK.EXE`) **[API]**; `AX=0200h` mluví znak (`SAY.COM`)
  **[binárka]**
- `62h` čtení řeči (AAPI) **[API]**
- `07h` — `VOICE.SYS` ho nastaví na obsluhu, která přeskočí 2 bajty
  (`VOICE.SYS+01C5h`) **[binárka]**. Je to past ESC 80186 pro instrukce
  koprocesoru (bit 15 relokačního registru PCB), což sedí s kódem MAME
  `i186.cpp` (HANDOFF 6.12); že Aria ten bit nastavuje, je zatím
  **[předpoklad]**.
- `69h` — služba **`VOICE.SYS`**, správce okna D000h přes latche `0580h`
  a `0480h` (`VOICE.SYS+01ADh`; CH 0 uložit, 1 obnovit, 2 stránka flash,
  3 stránka karty, 4 vypnout; HANDOFF 6.2) **[binárka]**. Není to služba
  boot ROM, jak se předpokládalo zpočátku (HANDOFF 6.1).
- `VOICE.SYS` je zároveň znakové zařízení `$VOICE` (atribut 8000h):
  čtení z `INT 16h`, zápis mluví přes `INT 61h AX=0200h` **[binárka]**.
- `62h` — obsluha v AAPI (`AAPI.EXE+A8D8h`): `AX=0200h` kopie z flash
  oknem, `AX=0201h` adresa kopírovací rutiny **[binárka]**, odpovídá
  `AAPI_MEMCPY_SPEECH` v ARIA-INT.H **[API]**.
