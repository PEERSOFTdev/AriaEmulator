# Aria Emulator

Emulátor kapesního počítače pro nevidomé **Aria** (Robotron, nástupce
Eureky A4) pro Windows. Cílem je spouštět původní programy Arie z obrazu
její interní flash — včetně řečového enginu PeterTalk na emulovaném DSP —
takže hlas zní stejně jako na skutečném stroji.

**Stav: vývoj.** Emulátor spustí Arii z obrazu disku B: až do hlavního
menu a **je slyšet**: úvodní řeč PeterTalk a harfa vycházejí z původních
programů na emulovaném DSP. Klávesnice hostitele hraje klávesy Arie a
okno se ovládá stejně jako emulátor Eureky A4 (viz níže). Stav stojí
v titulku okna (NVDA+T). Co je zjištěno a co je otevřené, popisuje
`HANDOFF.md`, mapu hardwaru `hardware-map.md`.

## Co emulátor potřebuje

Firmware Arie není součástí projektu (viz `ROM-NOTICE.txt`). Je to obsah
interní flash, tedy disku B:, a dodává se jako aktualizace od Robotronu.

Dále potřebuje **jádro FreeDOS**, které nahrazuje ztracený ROM-DOS: soubor
`KERNEL.SYS` z vydání ke2046, sestavení pro 8086 (`ke2046_86f16.zip` na
GitHubu projektu FDOS/kernel). Je pod licencí GNU GPL a s emulátorem se
nespojuje, jen se načte jako samostatný soubor. Emulátor ho hledá ve
složce z proměnné `ARIA_FREEDOS`, jinak vedle sebe; jinou cestu lze zadat
volbou `--jadro`.

ROM-DOS na Arii běžel z paměti ROM a programům nebral žádnou paměť.
Emulátor proto při startu přesune rezidentní kód jádra FreeDOS (40 KB)
do oblasti ROM nad RAM diskem, stejně jako to jádro samo dělá s oblastí
HMA na počítačích PC, a DOS dostane 704 KB. Textový editor tak má
dost paměti i s výchozím nastavením, jako na skutečné Arii. Soubor
`KERNEL.SYS` se nemění, úprava se děje jen v paměti emulovaného stroje
a jen u sestavení ke2046, které emulátor zná.

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

## Klávesnice

Klávesy Arie jsou rozložené stejně jako v emulátoru Eureky A4:

- `F`, `D`, `S` jsou body 1, 2, 3 a `J`, `K`, `L` body 4, 5, 6;
  mezerník je mezerník. Akord platí, až jsou všechny jeho klávesy
  stisknuté současně.
- `F1` až `F8` jsou funkční klávesy Arie.
- Šipky jsou kurzorový kříž a `Shift` je kulatá klávesa uprostřed něj;
  samotný ztiší řeč. `Home`, `End`, `PgUp` a `PgDn` stisknou dvě šipky
  naráz (například `Home` je nahoru a vlevo).
- Písmena se nepíšou, píše se body, jako na Arii.

Ovladač klávesnice Arie bere akord, až se nezmění po tři snímání
klávesnice. Emulátor proto drží všechny stisknuté klávesy, dokud od
posledního stisku neuběhne pět snímání (asi 0,07 sekundy, HANDOFF
6.4), takže i rychle zahraný akord dojde celý. Automatické opakování
Windows se nepoužívá.

## Okno a nabídka

Okno se ovládá stejně jako emulátor Eureky A4. Arii berou jen tři
klávesy, které stejně nemá:

- **`F12`** otevře nabídku. `Alt` ani `F10` ji neotevřou, dokud má
  klávesnici Aria, aby náhodný `Alt` nepřepnul do nabídky potichu.
- **`F11`** pustí do Windows **jednu následující klávesu** nebo zkratku.
  Vysoký tón znamená, že je nachystaná, nižší, že se minula.
- **`Shift+F11`** uvolní klávesnici **úplně**: do Arie nejde nic a okno
  se chová jako každé jiné okno Windows. Klesající dvojice tónů znamená,
  že klávesy Arii opouštějí, stoupající, že se vracejí. Stav stojí
  i v titulku okna.

Tóny jsou stejné jako v emulátoru Eureky. Zkratky s `Ctrl` platí po
`F11`, nebo rovnou, když je klávesnice uvolněná: `Ctrl+Q` ukončí
emulátor, `Ctrl+H` vypíše klávesové zkratky. Bez `F11` jdou písmena do
Arie jako body, takže `Ctrl+D` zkratkou není.

**Rozdíl proti Eurece:** `Alt+F4` okno zavře vždy, protože Aria klávesu
Alt nemá (na Eurece je `Alt+F4` klávesa stroje).

Klávesy, které Aria drží, když okno ztratí fokus, když se otevře nabídka
nebo když se klávesnice uvolní, emulátor pustí, aby nezůstaly stisknuté.

## Zapnutí, vypnutí a paměť

Paměť Arie včetně RAM disku `D:` a nastavení hodin drží baterie, takže
vypnutí ani teplý reset je nesmažou. Emulátor to dělá stejně a navíc
paměť uloží mezi spuštěními:

- **`F11`, `Ctrl+V`** (Stroj → Vypnout Arii) Arii vypne. Vypnutá zůstane
  v okně: ohlásí to **tři klesající tóny** a titulek, a klávesnici dostanou
  Windows. Arie se sama vypíná stejně, zápisem do portu `0701h`.
- **`F11`, `Ctrl+P`** (Teplý reset) vypnutou Arii zapne a běžící spustí
  znovu, paměť zůstane. Ohlásí to **tři stoupající tóny**.
- **`F11`, `Ctrl+R`** (Reset) **odpojí baterii**: paměť i RAM disk se
  ztratí. Emulátor se nejdřív zeptá — na rozdíl od Eureky, protože na
  RAM disku jsou soubory.

**Zavření okna:** když je Aria vypnutá, paměť se tiše uloží a příští
spuštění na ni naváže. Když běží, emulátor se zeptá: **Ano** ji vypne a
paměť uloží, **Ne** zavře bez uložení (příště prázdná paměť, jako po
odpojení baterie), **Storno** nechá Arii běžet.

Paměť leží v souboru `pamet-XXXXXXXX.bin`, kde `XXXXXXXX` je kontrolní
součet obrazu disku B:. Každé sestavení firmwaru (například 7.05 a 6.87)
má tak svou paměť a přepnutí mezi nimi žádnou nesmaže. Soubor je ve složce
`config` vedle `AriaEmulator.exe`, když ji vytvoříte (přenosný režim),
jinak v `%APPDATA%\Aria`. Při startu se přečte a smaže, takže běh, který
skončí jinak než vypnutím, nechá příště prázdnou paměť.

Arii vypnete i jejím vlastním příkazem **konec**: v hlavní nabídce
všechny čtyři šipky naráz. Aria řekne „vypnuto“ a vypne se. Většina
klávesnic PC čtyři šipky naráz neohlásí (nezvládne tolik kláves
najednou), proto je konec i v nabídce **Klávesnice → Poslat Arii
klávesu → Konec** (`F12`, `K`, `P`, `K`); stačí i `Home` s `PgDn`, které
stisknou po dvou šipkách.

Po odpojení baterie (a dřív při každém spuštění emulátoru, protože
paměť nepřežila) se hlavní nabídka zeptá „ram disk může být poškozen,
formátovat, ano nebo ne?“. Odpovídá se **`y`** (body 1, 3, 4, 5, 6),
i v české verzi; ne je `n`. Dokud otázka visí, bere všechny klávesy,
takže ani konec nefunguje. Protože se teď paměť drží, otázka po
vypnutí a zapnutí už nepřijde.

## Zvuk

Zvuk jde do výchozího zvukového zařízení Windows. Když se ho nepodaří
otevřít, emulátor pípne a do titulku připíše „bez zvuku“; Arie to nevadí,
běží dál. Vzorkovací frekvence řeči Arie se nedochovala ani v dokumentaci;
emulátor používá 32 kHz pro českou řeč, podle poslechu (HANDOFF 6.3).

## Licence

MIT, viz `LICENSE.txt`. Firmware a technická dokumentace Arie pod ni
nespadají.
