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

- CPU 80C188 **[web]**.
- Řídicí blok periferií (PCB) je přemístěn do I/O na adresu `0000h`
  **[binárka]**: registry na offsetech podle Intelu sedí s tím, co
  programy dělají:
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

- 1 MB nevolatilní RAM, 2 MB flash **[web]**; stránkování neznámé
  (HANDOFF 6.2).
- Disk B: = flash se souborovým systémem FAT12 Datalight **[binárka]**.

## Porty desky (předběžně)

- `0401h`, `0402h` — zápis z `SPEAK.EXE`, nejspíš DSP **[předpoklad]**
- `0480h` — čtou `BKB.COM`, `DLFFSA.EXE`, `TRICKSS2.EXE`, zapisuje
  `VOICE.SYS`
- `050Ch`, `050Dh` — čtou `ARIALARM.EXE`, `BKB.COM`
- `057Eh`, `057Fh` — čtou `ATKBD.COM`, `TRICKSS2.EXE`
- `0580h` — **sdílený latch** (flash i hlas), správce `VOICE.SYS` přes
  `INT 69h`; dřív do něj přímo zapisoval `FLASHDSK.SYS` z boot ROM, který
  VOICE.SYS přepisuje (`VOICE.SYS+0204h`) **[binárka]**; zapisuje i
  `RAMDISK.EXE`
- `0581h`, `0584h` — čte `BKB.COM`
- `0600h`, `0601h`, `0681h` — `FOSSIL.COM`, nejspíš UART **[předpoklad]**
- `0700h` — čte `BKB.COM`, nejspíš klávesnice **[předpoklad]**
- `0701h` — zápis 0 vypne stroj, `TURNOFF.COM+0` **[binárka]**
- `0702h` — zapisují `DLFFSA.EXE`, `TRICKSS2.EXE`, nejspíš flash
  **[předpoklad]**

## Softwarová přerušení

- `60h` Aria API (`AAPI.EXE`) **[API]**
- `61h` řeč (`SPEAK.EXE`) **[API]**; `AX=0200h` mluví znak (`SAY.COM`)
  **[binárka]**
- `62h` čtení řeči (AAPI) **[API]**
- `07h` — `VOICE.SYS` ho nastaví na obsluhu, která přeskočí 2 bajty
  (`VOICE.SYS+01C5h`); nejspíš past ESC pro instrukce koprocesoru
  **[binárka]**, význam **[předpoklad]**
- `69h` — služba **`VOICE.SYS`** pro latch `0580h` (`VOICE.SYS+01ADh`,
  větvení podle `CH` = 0 až 5; funkce 2 zapisuje `DL`) **[binárka]**. Není
  to služba boot ROM, jak se předpokládalo zpočátku (HANDOFF 6.1).
