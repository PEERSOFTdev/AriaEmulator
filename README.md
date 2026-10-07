# Aria Emulator

Emulátor kapesního počítače pro nevidomé **Aria** (Robotron, nástupce
Eureky A4) pro Windows. Cílem je spouštět původní programy Arie z obrazu
její interní flash — včetně řečového enginu PeterTalk na emulovaném DSP —
takže hlas zní stejně jako na skutečném stroji.

**Stav: vývoj.** Emulátor spustí Arii z obrazu disku B: až do hlavního
menu a **je slyšet**: úvodní řeč PeterTalk a harfa vycházejí z původních
programů na emulovaném DSP. **Klávesy zatím do Arie nevedou**, takže se
s ní nedá nic dělat; okno se zavírá Alt+F4. Stav stojí v titulku okna
(NVDA+T). Co je zjištěno a co je otevřené, popisuje `HANDOFF.md`, mapu
hardwaru `hardware-map.md`.

## Co emulátor potřebuje

Firmware Arie není součástí projektu (viz `ROM-NOTICE.txt`). Je to obsah
interní flash, tedy disku B:, a dodává se jako aktualizace od Robotronu.

Dále potřebuje **jádro FreeDOS**, které nahrazuje ztracený ROM-DOS: soubor
`KERNEL.SYS` z vydání ke2046, sestavení pro 8086 (`ke2046_86f16.zip` na
GitHubu projektu FDOS/kernel). Je pod licencí GNU GPL a s emulátorem se
nespojuje, jen se načte jako samostatný soubor. Emulátor ho hledá ve
složce z proměnné `ARIA_FREEDOS`, jinak vedle sebe; jinou cestu lze zadat
volbou `--jadro`.

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

## Zvuk

Zvuk jde do výchozího zvukového zařízení Windows. Když se ho nepodaří
otevřít, emulátor pípne a do titulku připíše „bez zvuku“; Arie to nevadí,
běží dál. Vzorkovací frekvence řeči Arie se nedochovala ani v dokumentaci;
emulátor používá 32 kHz pro českou řeč, podle poslechu (HANDOFF 6.3).

## Licence

MIT, viz `LICENSE.txt`. Firmware a technická dokumentace Arie pod ni
nespadají.
