# Právní postavení projektu

> **Summary in English.** The Aria Emulator is an independent work that
> reproduces the functionality of the Robotron Aria hardware and replaces
> its missing boot ROM with its own code. It contains no firmware of the
> machine, in any form, and none of its releases ever will. The firmware
> (the contents of the internal flash, drive B:) is a copyright work of
> Robotron Pty. Ltd. (Australia) and of the third parties whose programs
> it includes. It was distributed to Aria users as update packages; the
> project does not distribute them and does not say where to obtain them.
> The emulator runs only firmware that its user supplies. The project
> relies on the exceptions for lawful users of computer programs in Czech,
> Slovak and Australian law and in Directive 2009/24/EC. The technical
> material for developers is not part of the project; facts about the
> machine's interfaces are described in the project's own words. The
> binding text of this statement is the Czech one below.

Toto prohlášení popisuje, co projekt Aria Emulator obsahuje a co ne, na
jakém právním základě stojí, co podle uvedených předpisů smí oprávněný
uživatel firmwaru a k čemu se zavazuje projekt sám.

## 1. Předmět a rozhodné právo

1.1. Prohlášení se vztahuje na repozitář projektu, na všechna jeho vydání
a na nástroje, které projekt poskytuje.

1.2. Prohlášení vychází z těchto předpisů:

- zákon č. 121/2000 Sb., o právu autorském, o právech souvisejících
  s právem autorským a o změně některých zákonů (autorský zákon), Česká
  republika (dále „český autorský zákon“);
- zákon č. 185/2015 Z. z. Autorský zákon, Slovenská republika (dále
  „slovenský autorský zákon“);
- Copyright Act 1968, Australský svaz (dále „australský Copyright Act“);
- směrnice Evropského parlamentu a Rady 2009/24/ES o právní ochraně
  počítačových programů (dále „směrnice“), kterou oba uvedené autorské
  zákony přejímají.

1.3. O porušení autorského práva se rozhoduje podle práva státu, ve
kterém se ochrana požaduje. Zpřístupnění na internetu je úkonem ve všech
státech zároveň. Prohlášení proto popisuje postavení, které obstojí podle
každého z uvedených právních řádů současně.

## 2. Emulátor

2.1. Emulátor je samostatné dílo autorů projektu. Šíří se pod licencemi
uvedenými v `LICENSE.txt`. Části od třetích stran se svobodnou licencí,
jádra procesoru 80C188 a signálového procesoru ADSP-2105 z projektu MAME
pod licencí BSD-3-Clause, jsou označeny tam, kde se nacházejí.

2.2. Emulátor napodobuje funkci technického vybavení počítače Aria:
procesor, signálový procesor, paměť, vstupně-výstupní porty, časovače,
přerušení, klávesnici, zvukový výstup, hodiny, slot pro paměťové karty
a vnější rozhraní. Funkce počítačového programu či zařízení, myšlenky
a principy, na nichž je založen kterýkoli prvek programu včetně jeho
rozhraní, programovací jazyk a formát datových souborů nejsou předmětem
autorskoprávní ochrany:

- český autorský zákon, § 65 odst. 2;
- slovenský autorský zákon, § 5 písm. a) a § 87 odst. 1;
- směrnice, čl. 1 odst. 2;
- rozsudek Soudního dvora EU C-406/10 *SAS Institute*.

2.3. Zaváděcí paměť ROM počítače Aria (BIOS, jádro ROM-DOS a ovladač
interní flash) není k dispozici. Emulátor ji nahrazuje vlastním kódem,
který odpovídá na volání programů z disku B: podle rozhraní těchto
volání, jak je programy používají, ne podle implementace, kterou projekt
nikdy neměl. Zdrojový kód emulátora neobsahuje firmware počítače Aria ani
jeho část, překlad, úpravu nebo přepis do jiného jazyka.

2.4. Místo ztraceného jádra ROM-DOS emulátor zavádí jádro FreeDOS, které
je pod licencí GNU GPL. Repozitář ani vydání ho neobsahují; dodává ho ten,
kdo emulátor spouští, jako samostatný soubor, který emulátor nemění.

## 3. Firmware

3.1. Firmwarem se zde rozumí obsah interní paměti flash počítače Aria,
tedy disk B:, a aktualizační balíky, které ho obsahují. Je to soubor
počítačových programů chráněných autorským právem. Majetková práva
k programům Robotronu pravděpodobně vykonává společnost Robotron Pty. Ltd.
(Austrálie) jako zaměstnavatel jejich autorů:

- český autorský zákon, § 58 odst. 1 a 7;
- slovenský autorský zákon, § 90 odst. 1 a 4;
- australský Copyright Act, s 35(6).

Společnost Robotron Pty. Ltd. je podle australského rejstříku (Australian
Business Register) a rejstříku ASIC nadále zapsaná jako činná (ověřeno
18. 10. 2026). Projekt nemá vědomost o tom, zda majetková práva k firmwaru
převedla na jinou osobu.

3.2. Disk B: obsahuje také programy a části programů třetích stran,
zejména společností Datalight (ROM-DOS, CardTrick, FlashDisk, VDISK),
PKWARE (rozbalovací kód PKLITE) a Borland (běhové knihovny) a řečový
engine PeterTalk, jehož autorem je podle údajů v něm Peter Veprek. Komu
náležejí majetková práva k jednotlivým částem, projekt neověřoval; pro
toto prohlášení na tom nezáleží, protože se k nim chová stejně jako
k firmwaru celému.

3.3. Majetková práva trvají po dobu života autora a 70 let po jeho smrti
(český autorský zákon § 27 odst. 1, slovenský autorský zákon § 32
odst. 1, australský Copyright Act s 33(2)). Stáří firmwaru, skutečnost,
že se stroj už nevyrábí, ani to, že aktualizace byly kdysi volně
k dispozici, na trvání ochrany nic nemění.

3.4. Repozitář ani žádné vydání projektu firmware neobsahuje, a to
v žádné podobě: ani jako samostatný soubor, ani zakódovaný,
zkomprimovaný, rozdělený na části, vyjádřený jako rozdíl vůči jinému
souboru, vložený do spustitelného souboru, ani tak, že by ho emulátor
nebo nástroj projektu sám stáhl z jiného zdroje. Každá z těchto podob je
rozmnoženinou téhož programu.

3.5. Žádné ustanovení uvedených předpisů nedovoluje firmware šířit bez
souhlasu nositelů práv. Na tom nic nemění nekomerční povaha projektu ani
to, že firmware byl dříve uživatelům Arie k dispozici. To, že ho nositel
práv sám zpřístupnil, dává oprávnění těm, jimž ho určil, ne tomu, kdo by
ho šířil dál.

3.6. Totéž platí pro všechno, co z firmwaru vznikne nástroji projektu
nebo jinak, zejména pro:

- programy z disku B: rozbalené z komprese PKLITE;
- výpisy paměti emulovaného stroje a uloženou paměť mezi spuštěními (je
  v ní zavedený kód programů z disku B:);
- proud bajtů posílaných signálovému procesoru a výpisy jeho programu;
- úplné nebo podstatné výpisy disassembly a zvuky, texty a tabulky
  z firmwaru vyňaté v úplném nebo podstatném rozsahu.

Takové soubory zůstávají u toho, kdo je vytvořil, mimo repozitář.
Jednotlivá krátká hlášení, kterými stroj odpovídá, projekt uvádí tam, kde
jimi označuje chování, které emulátor napodobuje nebo které testy ověřují.
Kontrolní součet, délka a jméno souboru s firmwarem rozmnoženinou programu
nejsou a projekt je uvádí, aby si každý mohl ověřit svůj vlastní soubor.

## 4. Firmware u uživatele

4.1. Emulátor pracuje jen s firmwarem, který mu dodá jeho uživatel.
Aktualizační balík čte tak, jak ho nositel práv dodával, bez úprav.

4.2. Firmware se uživatelům Arie dodával jako aktualizační balíky
k přenesení do přístroje. Projekt firmware nešíří, neuvádí, kde ho získat,
a na žádný jeho zdroj neodkazuje. Neposuzuje ani to, zda je konkrétní
uživatel jeho oprávněným uživatelem; to určuje zákon a podmínky, za
kterých firmware získal (český autorský zákon § 66 odst. 6 první věta,
slovenský autorský zákon § 89 odst. 1, směrnice čl. 5 odst. 1).

4.3. Oprávněný uživatel smí bez souhlasu nositele práv:

- zhotovit záložní rozmnoženinu, je-li potřebná pro využití programu
  (český autorský zákon § 66 odst. 1 písm. c), slovenský autorský zákon
  § 89 odst. 2 písm. b), směrnice čl. 5 odst. 2);
- podle australského Copyright Act zhotovit z původního exempláře
  rozmnoženinu výhradně pro vlastní použití, aby ji používal místo
  původního exempláře, který si ponechá (s 47C(1)(c)(i)).

Tato oprávnění nelze smlouvou vyloučit ani omezit (český autorský zákon
§ 66 odst. 6 druhá věta, směrnice čl. 8 druhý pododstavec, australský
Copyright Act s 47H).

Při spuštění firmwaru v emulátoru se projekt opírá o oprávnění zhotovit
rozmnoženinu, která je nezbytná k využití programu v souladu s jeho
určením (český autorský zákon § 66 odst. 1 písm. b), slovenský autorský
zákon § 89 odst. 2 písm. a), směrnice čl. 5 odst. 1). Toto oprávnění
platí, není-li dohodnuto jinak, a není ustálené, zda se vztahuje na běh
mimo stroj, pro který byl firmware určen.

4.4. Projekt se opírá jen o tato ustanovení, ne o výjimku pro osobní nebo
soukromou potřebu. Český autorský zákon výslovně považuje za užití i užití
počítačového programu pro osobní potřebu fyzické osoby (§ 30 odst. 3).

4.5. Ustanovení bodu 4.3 dovolují rozmnoženinu jen k využití programu
tím, kdo ji zhotovil. Její předání jiné osobě nedovoluje žádné z nich, a
to ani jinému uživateli Arie, ani k zálohování nebo testování u jiného
(bod 3.5). Totéž platí pro soubory podle bodu 3.6.

## 5. Technický materiál pro vývojáře

5.1. Materiál Robotronu pro vývojáře aplikací Arie (*Aria API Reference*,
specifikace řečových sekvencí PeterTalk, hlavičkové soubory, zdrojové
texty ukázkového programu, přeložené knihovny a verze `AAPI.EXE` pro PC)
je chráněn autorským právem. Hlavičkové soubory, ukázkový program,
knihovny a `AAPI.EXE` jsou počítačové programy.

5.2. Repozitář ani vydání neobsahují tento materiál, podstatné části jeho
textu nebo tabulek, ani žádný jeho soubor. Nástroje projektu, které ho
potřebují, ho hledají mimo repozitář, u toho, kdo je spouští.

5.3. Autorské právo chrání vyjádření materiálu, ne technické skutečnosti,
které sděluje. Čísla přerušení a funkcí, jména funkcí a konstant, seznamy
parametrů, řídicí sekvence řeči a formáty souborů jsou popisem rozhraní
stroje a jeho programů (bod 2.2). Projekt je používá v kódu a popisuje je
vlastními slovy s odkazem na pramen. Z programů podle bodu 5.1 projekt
necituje (český autorský zákon § 66 odst. 7).

## 6. Zkoumání firmwaru

6.1. Oprávněný uživatel smí pozorovat, zkoumat a zkoušet funkčnost
programu za účelem zjištění myšlenek a principů, na nichž je založen, a to
při jeho zavedení, zobrazení, provozu, přenosu nebo ukládání:

- český autorský zákon, § 66 odst. 1 písm. d);
- slovenský autorský zákon, § 89 odst. 2 písm. c);
- směrnice, čl. 5 odst. 3;
- australský Copyright Act, s 47B(3).

Většina poznatků projektu vznikla takto, pozorováním běžícího firmwaru
v emulátoru. Zveřejnění takových poznatků není omezeno, protože popisují
myšlenky a principy, ne vyjádření programu.

6.2. Rozmnožení kódu nebo překlad jeho formy (disassembly) je přípustné,
je-li nezbytné k získání informací potřebných k dosažení interoperability
nezávisle vytvořeného programu, tyto informace nebyly dříve snadno
dostupné a týká se jen potřebných částí programu:

- český autorský zákon, § 66 odst. 1 písm. e);
- slovenský autorský zákon, § 89 odst. 3;
- směrnice, čl. 6;
- australský Copyright Act, s 47D.

Nezávisle vytvořeným programem je emulátor a firmware je program, se
kterým spolupracuje. Protože zaváděcí ROM chybí, je to často jediná
cesta, jak zjistit, co od ní programy z disku B: očekávají (bod 2.3).

6.3. Informace získané podle bodu 6.2 projekt používá jen
k interoperabilitě emulátora s firmwarem a zveřejňuje je jen v rozsahu,
který je k ní třeba. Nepoužívá je k vytvoření programu podobného
firmwaru ve vyjádření.

6.4. Projekt nezveřejňuje výpisy disassembly firmwaru. Dokumentace
obsahuje nanejvýš krátké úryvky několika instrukcí a adresy ve tvaru
`SOUBOR.EXT+offset` tam, kde bez nich nelze popsat, jak firmware ovládá
technické vybavení stroje, nebo doložit, kdo které rozhraní používá.

## 7. Příspěvky

7.1. Projekt nepřijímá příspěvky, které obsahují:

- firmware nebo jeho část v jakékoli podobě podle bodu 3.4 nebo soubory
  podle bodu 3.6;
- výpisy disassembly firmwaru nad rozsah bodu 6.4;
- bajty firmwaru jako testovací data;
- kód přepsaný z firmwaru nebo z jeho disassembly do jiného jazyka;
- text, tabulky nebo soubory z materiálu podle bodu 5.1 nad rozsah
  bodu 5.3.

7.2. Testy pracují s programy, které napsali autoři projektu, s daty pod
svobodnou licencí, která si spouštějící sám stáhne, nebo s firmwarem,
který dodá ten, kdo testy spouští, a který repozitář neobsahuje.

7.3. Autor příspěvku odpovídá za to, že k němu má práva potřebná k jeho
šíření pod licencí projektu.

7.4. Poznatky projektu, které se opírají o firmware, pocházejí od
vlastníka Arie, který aktualizace získal tak, jak se uživatelům Arie
dodávaly. Další takové poznatky projekt přijímá od toho, kdo je získal
způsobem podle bodu 6 na firmwaru, jehož je oprávněným uživatelem.
Hlášení chyb popisuje, co emulátor udělal, a uvádí verzi
firmwaru; soubory podle bodu 3.6 nepřikládá.

## 8. Závěrečné ustanovení

Toto prohlášení popisuje právní postavení projektu a pravidla, kterými se
řídí. Nikomu nepřiznává oprávnění nad rámec, který mu dává zákon, a
nenahrazuje posouzení konkrétního případu.
