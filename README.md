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
nespojuje, jen se načte jako samostatný soubor. Stačí ho položit vedle
`AriaEmulator.exe`. Emulátor ho hledá ve složce z proměnné `ARIA_FREEDOS`,
jinak vedle sebe, o složku výš a v pracovní složce; jinou cestu lze zadat
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

**Nejjednodušší je položit aktualizaci vedle `AriaEmulator.exe`** a
spustit ho bez parametrů. Emulátor ji najde sám, nic se nevybírá
v dialogu, protože Aria běží hned od spuštění. Hledá:

1. firmware z minulého spuštění, dokud jde načíst (pamatuje si ho
   v souboru `nastaveni.txt`, řádek `disk-b`),
2. jinak vedle `AriaEmulator.exe`, potom o složku výš a nakonec
   v pracovní složce, v každé z nich i v podsložkách `CARD` a `SERIAL`,
   jak je měla sada aktualizací od Robotronu.

Soubory se poznávají podle obsahu, ne podle jména: vezme se každý archiv
ZIP s `FLASHDSK.IMG` nebo díly `ARIAA4`, samotný `FLASHDSK.IMG` a díl
`ARIAA4.1`. Jiné archivy ZIP vedle emulátoru (například s jádrem FreeDOS)
nevadí. Obrazy karet `*.IMG` se za disk B: nepovažují. Prohledávání
skončí u první složky, ve které se něco najde; je-li v ní (včetně jejích
podsložek `CARD` a `SERIAL`) několik verzí, vyhraje nejnovější bez ohledu
na jazyk, takže anglická 7.04 přebije českou 6.87.

Jinou verzi spustíte jednou s volbou `--flash` a emulátor si ji
zapamatuje, takže příště ji vezme i bez ní:

    AriaEmulator.exe --flash D:\aria\update\SERIAL\CSR0687.ZIP

Verzi a kódování spuštěného firmwaru hlásí titulek okna (NVDA+T).
Když se firmware nenajde, emulátor to řekne, vyjmenuje prohledané složky
a nespustí se.

Kódování lze vnutit volbou `--kodovani kamenicky` nebo
`--kodovani cp852`.

Pro vývoj: proměnná prostředí `ARIAFLASH` platí jako `--flash`. Když se
nic nenajde ani vedle emulátoru, prohledá se ještě složka z proměnné
`ARIAFW` (výchozí `D:\aria\update`).

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

### Externí klávesnice

K Arii se dala připojit obyčejná klávesnice IBM přes adaptér na sériovém
portu; Aria pak bere psaní z ní i z braillských kláves zároveň (HANDOFF
6.6). V emulátoru:

1. V Arii v Parametrech (mezerník+A nebo F9) zapněte volbu **Podpora
   externí klávesnice**. Aria si ji ukládá do `D:\ARIA.INI` na RAM disku
   (jako `External Keyboard=On` v sekci `System`), takže po odpojení
   baterie je zase vypnutá.
2. **Klávesnice → Připojit externí klávesnici.** Klávesnice PC hned píše
   jako externí klávesnice: písmena, české znaky podle rozložení QWERTZ
   v ovladači Arie, `F1` až `F10`, Caps Lock s tónem Arie. Nastavení si
   emulátor pamatuje.
3. **Teplý reset** (`F11`, `Ctrl+P`): ovladač `ATKBD.COM` hledá adaptér
   jen při startu. Pak řekne „External keyboard driver installed OK“.
   Bez volby v Parametrech řekne „Disabled.“ a klávesy nikam nevedou.

**`F11`, `Ctrl+K`** přepíná klávesnici PC mezi externí klávesnicí
a braillskými klávesami (dva krátké vysoké tóny = externí, dva nízké =
braillské); adaptér zůstává připojený, jako by na stole ležely obě
klávesnice. V titulku okna stojí „externí klávesnice“, když píše externí.
Akordy s mezerníkem a body jdou jen z braillských kláves; `Ctrl+K` bez
`F11` jde do Arie jako klávesa.

- **Escape je `Shift+Esc`.** Samotný `Esc` nemá ovladač Arie v tabulce
  a Aria na něj pípne; `Shift+mezerník` tu je jen mezera.
- Šipky, `Home`, `End`, `PgUp`, `PgDn`, `Insert`, `Delete` a `F1` až
  `F12` jdou jako klávesy Arie, se `Shiftem` jako jejich druhé významy.
- Dokud ovladač externí klávesnici nepřevezme (před teplým resetem, bez
  volby v Parametrech, nebo když adaptér nenašel), **každý stisk pípne**
  krátkým hlubokým tónem — klávesy by jinak nikam nevedly — a titulek
  okna říká „externí klávesnice, Aria ji nebere“. Pak pomůže teplý
  reset, nebo `F11`, `Ctrl+K` zpět na braillské klávesy.
- Klávesy stisknuté během startu Arie se zahodí, dokud ovladač adaptér
  nepřevezme; jinak by je ovladač vzal za odpověď adaptéru.

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
Windows**, tak jako se do emulátoru Eureky vkládá disketa ze složky,
karta SRAM z obrazu, nebo **fleš karta** z obrazu `.flash`. Aria
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
  složka, celý. **Obraz s příponou `.flash` je fleš karta** (násobek
  2 MB, nejvýš 64 MB); ostatní jsou karty SRAM. Fleš karta drží data bez
  baterie, hlášení baterie na ni tedy nikdy neupozorní. Nový formát
  vznikne formátováním jednotky `C:`; se zapnutou podporou staré fleš
  karty (MODE) je tatáž karta i jednotkou `E:`, a formátováním `E:`
  vznikne starý formát (nejvýš 4 MB). Karta pak funguje jen na té
  jednotce, na které byla naformátovaná; druhá hlásí „celkové
  selhání“. Ze složky být fleš karta nemůže: její
  formát (FTL) si zakládá ovladač Arie.
- **`F11`, `Ctrl+M`** (Vložit novou kartu) založí novou kartu. V dialogu
  zvolíte druh a velikost (u SRAM předvybraný 1 MB) a pak, kam ji uložit:
  - **Nová prázdná složka** — pojmenujete novou složku; ta pak funguje
    jako každá karta ze složky.
  - **Obraz karty, naformátovaný** — soubor s prázdnou kartou, jmenovkou
    je jméno souboru.
  - **Obraz karty, nenaformátovaný** — soubor plný bajtů FFh, jako
    smazaná karta. Aria na ni nic nezapíše (zápis skončí chybou, bez
    otázky), dokud ji sama nenaformátuje; formátování v Arii funguje.
    Do té doby správce souborů ukazuje žádné soubory a kapacitu, která
    s velikostí karty nesouvisí — hlásí ji tak ovladač karty.
  - **Fleš karta, nenaformátovaná** — obraz s příponou `.flash`, velikost
    2, 4, 10 nebo 20 MB (předvybraná 4 MB). Je to fleš karta z čipů Intel
    28F008SA, jakou znají ovladače Arie; Aria ji musí naformátovat sama.
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

## Disketa (mechanika Backpack, jednotka E:)

Arie se dala připojit externí disketová mechanika **Micro Solutions
Backpack** 3,5" na paralelní port; emulátor ji má i s řadičem disket
a Aria s ní pracuje svým vlastním ovladačem `BACKPACK.SYS`. Disketa je
buď **složka ve Windows**, jako u karty, nebo **obraz**: soubor
s obsahem diskety sektor po sektoru (`.img`, `.ima`, `.vfd`, `.flp`,
`.dsk`), jak je dělají jiné emulátory a nástroje. Nabídka **Disketa**
(`F12`, `D`); zkratky jsou zkratky karty se `Shiftem` navíc:

- **Připojit mechaniku Backpack** zapojí kabel (zaškrtnuto) nebo ho
  vytáhne. `BACKPACK.SYS` hledá mechaniku jen při startu, proto ji Aria
  uvidí až po teplém resetu (`F11`, `Ctrl+P`) nebo dalším zapnutí; při
  startu pak řekne „The following BACKPACK drive is available: Drive E:“.
  Emulátor si to pamatuje. Bez mechaniky je jednotka `E:` starý formát
  fleš karty; s mechanikou se ten posune na další písmeno.
- **`F11`, `Ctrl+Shift+I`** (Vložit disketu ze složky) udělá ze složky
  disketu **1,44 MB** a jmenovku jí dá podle jména složky; na velikost se
  neptá. Zpět do složky se zapisuje stejně jako u karty ze složky: jen
  změněné soubory, nové pod jménem 8.3, smazané do `.aria-trash`.
  Složka, která se na disketu nevejde (víc než asi 1 423 KB, nebo víc
  než 223 položek v kořeni), se odmítne s důvodem. Když ji Aria
  přeformátuje na 720 KB, zůstane do vysunutí disketou 720 KB; příště
  přijde zase jako 1,44 MB.
- **`F11`, `Ctrl+Shift+O`** (Vložit disketu z obrazu) bere diskety
  **720 KB a 1,44 MB**. Velikost pozná podle zaváděcího sektoru, jinak
  podle velikosti souboru. **Kratší obraz** (uříznutý za posledním
  použitým sektorem, jak bývají v archivech) se doplní a uloží v plné
  velikosti. Obraz diskety 5,25" (360 KB, 1,2 MB) nebo větší než 1,44 MB
  mechanika odmítne a řekne proč. Obraz, který je ve Windows **jen pro
  čtení**, je zamčená disketa a nic se do něj nezapíše.
- **`F11`, `Ctrl+Shift+M`** (Vložit novou disketu) založí obraz
  naformátovaný na 1,44 MB nebo 720 KB, jak ho dělá FORMAT v DOSu,
  nenaformátovaný, nebo prázdnou složku. Nenaformátovaný naformátuje Aria: ve správci souborů `Shift+F7`,
  písmeno `e`, pak otázka „sedm set dvacet kilo“ (ano = 720 KB, ne =
  1,44 MB). Disketa 1,44 MB naformátovaná na 720 KB se stane disketou
  720 KB i s menším obrazem.
- **`F11`, `Ctrl+Shift+0`** disketu vysune a zapíše do obrazu;
  **`F11`, `Ctrl+Shift+Z`** ji zamkne proti zápisu. Aria pak řekne
  „e chráněn proti zápisu“. Prázdná mechanika hlásí „disk e není
  připraven“.
- Co Aria na disketu zapíše, uloží emulátor do obrazu sám asi sekundu po
  posledním zápisu, při vysunutí a při ukončení. Obraz ani složka, do
  kterých Aria nic nezapsala, se nezmění; kratší obraz zůstane kratší.
  Výměnu diskety Aria pozná hned,
  tak jako skutečná mechanika.
- **Sloty** jsou tytéž jako u karet, jen vlastní: **`F11`,
  `Ctrl+F1` až `Ctrl+F9`** a **Disketa → Spravovat sloty…**. Ne
  `Ctrl+Shift` s číslicí: ty si rády berou jiné programy pro sebe
  (správci schránky a klávesových zkratek) a do okna emulátoru by pak
  nedošly. Slot nese obraz i složku.

V titulku okna je disketa jako karta: „disketa Hry.img, 1,44 MB“, nebo
„bez diskety“, když je mechanika připojená a prázdná.

## Zvuk

Zvuk jde do výchozího zvukového zařízení Windows. Když se ho nepodaří
otevřít, emulátor pípne a do titulku připíše „bez zvuku“; Arie to nevadí,
běží dál. Vzorkovací frekvence řeči Arie se nedochovala ani v dokumentaci;
emulátor používá 32 kHz pro českou řeč, podle poslechu (HANDOFF 6.3).

## Rychlost procesoru

Procesor Arie, 80C188 na 15 MHz, má jen osmibitovou sběrnici: každý bajt
programu i dat po ní jde zvlášť, nejméně čtyři takty. Emulátor to od
17. 10. 2026 počítá podle katalogového listu Intelu (HANDOFF 6.12).
Opakování kláves a čekání v programech měří časovače, ty se nemění; řeč
zní stejně, i dlouhá při nejvyšší rychlosti (ověřeno nahrávkou
a poslechem). Zpomalí se to, co tempo počítá průchody smyčkou —
třeba hra Obránce, která předtím běžela asi 1,7× rychleji, než
osmibitová sběrnice dovoluje. Start do hlavní nabídky trvá asi 9 sekund
emulovaného času místo 6.
Čekací stavy paměti skutečné Arie neznáme, proto zůstávají nulové;
upřesní je až měření na skutečném přístroji.

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
- **„adaptér klávsenice není připojen“** — překlep v hlášce ovladače
  externí klávesnice (`ATKBD.COM+0509h`, sestavení 7.05), kterou Aria
  řekne, když adaptér při startu neodpoví.

## Licence

MIT, viz `LICENSE.txt`. Firmware a technická dokumentace Arie pod ni
nespadají.
