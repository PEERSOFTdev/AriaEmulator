# Aria Emulator

Emulátor kapesního počítače pro nevidomé **Aria** (Robotron, nástupce
Eureky A4) pro Windows. Cílem je spouštět původní programy Arie z obrazu
její interní flash — včetně řečového enginu PeterTalk na emulovaném DSP —
takže hlas zní stejně jako na skutečném stroji.

**Stav: začátek vývoje.** Emulátor zatím jen načte zvolený obraz disku B:
a oznámí, co v něm našel (zdroj, verzi a kódování). Co je zjištěno a co je
otevřené, popisuje `HANDOFF.md`, mapu hardwaru `hardware-map.md`.

## Co emulátor potřebuje

Firmware Arie není součástí projektu (viz `ROM-NOTICE.txt`). Je to obsah
interní flash, tedy disku B:, a dodává se jako aktualizace od Robotronu.

## Volba verze firmwaru

Disk B: si můžete vybrat. Emulátor přijme aktualizaci přesně tak, jak ji
Robotron dodával, bez rozbalování:

- kartovou aktualizaci, archiv ZIP se souborem `FLASHDSK.IMG`
  (například `CARD\CC0705.ZIP`),
- sériovou aktualizaci, archiv ZIP s díly `ARIAA4.1` až `ARIAA4.3`
  (například `SERIAL\CSR0687.ZIP`),
- samotný `FLASHDSK.IMG`, kterýkoli z dílů `ARIAA4.1` až `ARIAA4.3`,
  nebo složku, ve které leží.

Česká verze 6.87 (`SERIAL\CSR0687.ZIP`) používá kódování **Kamenických**,
stejně jako Eureka A4. Verze 6.99 a 7.05 používají **CP852 (Latin 2)**.
Kódování emulátor pozná z obsahu disku sám.

Spuštění:

    AriaEmulator.exe --flash D:\aria\update\SERIAL\CSR0687.ZIP

Bez `--flash` se použije proměnná prostředí `ARIAFLASH`, a když není
nastavená, `CARD\CC0705.ZIP` ve složce z proměnné `ARIAFW` (výchozí
`D:\aria\update`). Kódování lze vnutit volbou `--kodovani kamenicky` nebo
`--kodovani cp852`.

Boot ROM Arie (BIOS a jádro ROM-DOS) se nedochovala. Emulátor ji nahrazuje
vlastní implementací.

## Licence

MIT, viz `LICENSE.txt`. Firmware a technická dokumentace Arie pod ni
nespadají.
