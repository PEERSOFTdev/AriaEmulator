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

## Karta (jednotka C:)

Do slotu pro kartu PC Card se dá vložit **karta SRAM ze složky ve
Windows**, tak jako se do emulátoru Eureky vkládá disketa ze složky. Aria
ji vidí jako jednotku `C:` a pracuje s ní svými vlastními ovladači; soubory
z PC se tak dostanou do Arie a zpět. Nabídka **Karta** (`F12`, `A`):

- **`F11`, `Ctrl+I`** (Vložit kartu ze složky) se zeptá na složku a pak na
  **velikost karty**. Nabídne jen skutečné velikosti, na které se složka
  vejde (64 KB až 64 MB), a předvybere nejmenší z nich od 1 MB, aby měla
  Aria kam psát. Karta, která ve slotu už je, se nejdřív zapíše zpět do
  své složky.
- **`F11`, `Ctrl+O`** (Vložit kartu z obrazu) vloží kartu ze souboru,
  který nese obsah karty bajt po bajtu (obvykle `.img`). Velikost karty
  je velikost souboru; soubor jiné velikosti než skutečné karty (mocnina
  dvou od 64 KB do 64 MB) se odmítne. Obraz se zapisuje stejně jako
  složka, celý.
- **`F11`, `Ctrl+M`** (Vložit novou kartu) založí novou kartu. V dialogu
  zvolíte druh a velikost (předvybraný je 1 MB) a pak, kam ji uložit:
  - **Nová prázdná složka** — pojmenujete novou složku; ta pak funguje
    jako každá karta ze složky.
  - **Obraz karty, naformátovaný** — soubor s prázdnou kartou, jmenovkou
    je jméno souboru.
  - **Obraz karty, nenaformátovaný** — soubor plný bajtů FFh, jako
    smazaná karta. Aria na ni nic nezapíše (zápis skončí chybou, bez
    otázky), dokud ji sama nenaformátuje; formátování v Arii funguje.
    Do té doby správce souborů ukazuje žádné soubory a kapacitu, která
    s velikostí karty nesouvisí — hlásí ji tak ovladač karty.
- **`F11`, `Ctrl+0`** (Vysunout kartu) kartu vysune a zapíše do složky
  nebo do obrazu.
- **`F11`, `Ctrl+Z`** (Zamknout proti zápisu) přepne ochranu proti
  zápisu. Aria pak při zápisu řekne „c chráněn proti zápisu“ a čeká na
  `r` (zkusit znovu), `f` (zrušit) nebo `i` (ignorovat).

Dialogy s obrazy se otvírají ve složce, kde byl poslední obraz, jinak
vedle karty ve slotu nebo poslední vložené; tam se otvírá i dialog nové
složky.

Vložení ohlásí **dvě krátké stoupající tóny**, vysunutí **jeden nízký**,
zamknutí dlouhý nízký a odemknutí dlouhý vysoký — stejně jako diskety
v emulátoru Eureky. Titulek okna říká, která karta je ve slotu, její
velikost a zda je zamčená, nebo „bez karty“.

Karta se do složky zapisuje **sama, průběžně**: jakmile do ní Aria
zapíše a pak se na sekundu odmlčí (jako diskety v emulátoru Eureky), a
dále při vysunutí a při ukončení emulátoru, ať se s pamětí stane cokoli.
Když se zápis do složky nepodaří, emulátor to řekne jednou a zkusí to
znovu po další změně a při vysunutí.

**Emulátor si karty pamatuje** (soubor `nastaveni.txt` vedle souboru
paměti): karta, která byla ve slotu při ukončení, se při dalším spuštění
vloží sama. Každá složka (a obraz) si navíc pamatuje svou velikost a zámek, jako
skutečná karta — při dalším vložení je její velikost v nabídce
předvybraná a zámek platí dál. Vysunutá karta se při dalším spuštění
nevkládá.

Co se zapisuje do složky:

- Jména souborů na kartě jsou ve tvaru DOSu 8.3, velkými písmeny
  a v kódování firmwaru, takže česká písmena zůstanou. Dlouhé jméno se
  zkrátí s `~1`, `~2` (`Prvni dopis.txt` je na kartě `PRVNID~1.TXT`).
  Podsložky se přenesou také. Jmenovka karty je jméno složky (nejvýš
  11 znaků, bez znaků, které Aria v jmenovce nedovolí).
- Atributy jen pro čtení, skrytý, systémový a archivovat jdou oběma
  směry: co je nastaveno ve Windows, má soubor i na kartě, a co Aria na
  kartě změní, dostane soubor ve složce při vysunutí, i když se jeho
  obsah nezměnil. Skrytá a systémová může být i podsložka.
- Soubor, který Aria jen četla, zůstane ve složce, jak byl. Změněný
  soubor si ponechá své původní dlouhé jméno. Nový soubor dostane jméno
  z karty.
- **Co Aria smazala, se nemaže**: přesune se do podsložky `.aria-trash`,
  odkud se dá vrátit. Ta podsložka na kartu nikdy nejde.
- Složka, která se nevejde ani na největší kartu, se odmítne.

Změny ve složce, které uděláte ve Windows, dokud je karta ve slotu, Aria
neuvidí — karta je kopie z chvíle vložení; uvidí je až po novém vložení.
Soubor, který mezitím změníte vy i Aria, dostane při zápisu verzi
z Arie.

### Sloty

Devět slotů vkládá karty jedním stiskem, jako sloty s disketami
v emulátoru Eureky: **`F11`, `Ctrl+1` až `Ctrl+9`**. Každý slot nese
složku nebo obraz karty. Karta, která už někdy ve slotu Arie byla, se
vloží hned se svou velikostí a zámkem; složka, která kartou ještě nebyla,
se nejdřív zeptá na velikost. Prázdný slot, nebo slot, jehož složka či
obraz zmizel, to řekne.

Sloty se plní v nabídce **Karta → Spravovat sloty…**. V seznamu je každý
slot s číslem (to je jeho zkratka) a se zámkem; tlačítka **Přiřadit
složku**, **Přiřadit obraz**, **Sem vloženou kartu** (karta, která je
právě ve slotu Arie) a **Vyprázdnit slot**. Zaškrtávátko **Zamknout tuto
kartu proti zápisu** patří kartě, ne slotu: dva sloty s touž kartou mají
jeden zámek a zámek platí i pro `Ctrl+I`. Na konci nabídky Karta je devět
položek pojmenovaných podle karet ve slotech.

## Zvuk

Zvuk jde do výchozího zvukového zařízení Windows. Když se ho nepodaří
otevřít, emulátor pípne a do titulku připíše „bez zvuku“; Arie to nevadí,
běží dál. Vzorkovací frekvence řeči Arie se nedochovala ani v dokumentaci;
emulátor používá 32 kHz pro českou řeč, podle poslechu (HANDOFF 6.3).

## Známé chyby firmwaru

Chyby, které dělá sám firmware Arie a které by udělala i skutečná Aria.
Emulátor je neopravuje, protože firmware nemění; každá je doložená místem
v programu (podrobnosti v HANDOFF).

- **Správa souborů hlásí miliony označených souborů** (sestavení 7.05).
  Po označení souborů řekne stav (`Shift+F8`) místo skutečného počtu
  obrovské číslo. Zpráva počítá s 32bitovým číslem, ale program jí dá jen
  16bitový počet a horní polovinu doplní, co zbylo v paměti
  (`FILEMGR.EXE+5B8Dh`, HANDOFF 6.8). Ostatní údaje stavu, například
  kolik místo soubory zabírají, jsou správně. Sestavení 6.87 říká počet
  správně.

## Licence

MIT, viz `LICENSE.txt`. Firmware a technická dokumentace Arie pod ni
nespadají.
