KARTA RATUNKOWA - AltScreen (mib2high-carplay-rgi) na K3346 / MU1438
====================================================================

Do czego: HMI nie wstaje po instalacji moda (logo w kolko, czarny ekran,
nie da sie wejsc do zielonego menu), wiec M.I.B. i zwykly rollback sa
niedostepne. Ta karta przywraca start HMI BEZ HMI i bez GEM.

Jak dziala: plik Swdlautorun.txt w katalogu glownym tej karty sprawia, ze
jednostka sama przechodzi w tryb aktualizacji (natywny MIBRoot - nie
potrzebuje HMI). W tym trybie M.I.B. 3.6.0 uruchamia Launcher/stuff.sh,
a ten - nasz ratunek/ratunek.sh, ktory:
  1. usuwa /mnt/app/eso/hmi/lsd/dsitracer_activated
  2. usuwa /mnt/app/eso/hmi/lsd/jars/mib2high-carplay-rgi.jar
  3. przywraca fabryczny traceConfig.properties (kopia z Twojego auta)
To sa wszystkie zmiany moda, ktore HMI czyta przy starcie.
Wzor: karta ratunkowa luka-dev, potwierdzona w aucie (issue #24).


TO MUSI BYC OSOBNA KARTA
------------------------
- NIE kopiuj tej karty na karte z M.I.B. ani na karte instalacyjna.
- NIE zostawiaj tej karty w aucie. Kazde wlozenie uzbrojonej karty
  uruchamia tryb aktualizacji.
- Na tej karcie NIE moze byc folderu mod\ z naszym instalatorem
  (command.sh) - M.I.B. w trybie aktualizacji by go wczytal.


UZBROJONA / ROZBROJONA
----------------------
Uzbrojona:   w katalogu glownym jest  Swdlautorun.txt
Rozbrojona:  w katalogu glownym jest  _Swdlautorun.txt  (z podkresleniem)

PO KAZDYM UZYCIU karta sama sie ROZBRAJA - M.I.B. zmienia nazwe na
_Swdlautorun.txt, zeby nie wpasc w petle aktualizacji.
Ponowne uzbrojenie: zmien nazwe _Swdlautorun.txt -> Swdlautorun.txt
(albo skopiuj karte jeszcze raz z work\sd_ratunek\ na komputerze).

=> Po TESCIE karta jest rozbrojona. Uzbroj ja przed instalacja moda.


TEST PRZED INSTALACJA MODA (na zdrowym aucie)
---------------------------------------------
Po co: sprawdzic, ze w TWOIM aucie tryb aktualizacji sie uruchamia
i ze nasz skrypt sie wykonuje - zanim bedzie potrzebny naprawde.
Na aucie bez moda skrypt niczego nie zmienia (usuwa pliki, ktorych nie
ma, i wpisuje traceConfig o identycznej tresci).

Ryzyko: takie samo jak przy instalacji M.I.B., ktora juz robiles -
jednostka ponownie instaluje M.I.B. 3.6.0 (te sama wersje).

1. Prostownik podlaczony. Zaplon wlaczony, MMI normalnie dziala.
2. Wloz te karte do SD1.
3. Czekaj. Tak jak przy instalacji M.I.B.: do ok. 60 s jednostka sama
   przejdzie w tryb aktualizacji i moze sie zrestartowac kilka razy.
   Jesli na ekranie pojawi sie menu aktualizacji z pytaniem - wybierz
   kontynuowanie (Continue / Weiter / Dalej).
   NIE wyjmuj karty w trakcie.
4. Gdy MMI wroci do normalnego ekranu - wyjmij karte.
5. Wloz karte do komputera i daj mi ja sprawdzic (tools\sprawdz_karte.ps1)
   albo sam otworz na karcie RATUNEK_LOG.txt. Na koncu przebiegu ma byc:
       WYNIK=OK (...)
       === RATUNEK koniec ===
       STUFF_KONIEC nosnik=... ratunek.sh=jest
   - jest WYNIK=OK          -> mechanizm dziala w Twoim aucie. Mozna instalowac.
   - tylko STUFF_KONIEC     -> M.I.B. zadzialal, ale ratunek.sh nie. NIE instaluj.
   - nie ma pliku           -> nic sie nie zapisalo. NIE instaluj.
   - WYNIK=NIEPEWNY         -> pokaz mi log przed instalacja.
   (Pierwsza wersja tej karty - test 1, 2026-10-20 - nie zostawila logu wcale.
   Przyczyny znalezione i usuniete; dlatego log zawiera teraz tez diagnostyke.)
6. Sprawdz, ze na karcie jest teraz _Swdlautorun.txt (rozbrojona) -
   to potwierdza, ze M.I.B. doszedl do konca.
7. UZBROJ karte ponownie (punkt wyzej) i odloz ja w bezpieczne miejsce.

Przy okazji testu sprawdz restart MMI (patrz nizej) - na zdrowym aucie
nic nie ryzykujesz, a bedziesz wiedzial, ze dziala.


GDY JEST BOOTLOOP (HMI nie wstaje po instalacji)
------------------------------------------------
0. Najpierw upewnij sie, ze to bootloop: zielone menu potrafi byc
   niedostepne do 5 minut po zimnym starcie. Odczekaj.
1. Prostownik podlaczony.
2. Wloz UZBROJONA karte ratunkowa do SD1.
3. Zaplon wlaczony. Czekaj - tryb aktualizacji ma sie uruchomic sam.
   U luka-dev ekran kilka razy przechodzil: logo -> czarny -> logo,
   i potrzebny byl twardy restart (patrz nizej). Potem pojawilo sie
   czerwone menu aktualizacji: najpierw Continue, potem Cancel.
4. Wyjmij karte. HMI powinno wstac normalnie.
5. Dopiero teraz zrob PELNY ROLLBACK zwyklym sposobem: karta
   INSTALACYJNA, skopiuj ZNACZNIKI\ROLLBACK do
   mod\mib2high-carplay-rgi-v8s-simple\, GEM -> m.i.b. -> run individual
   script. (Ratunek przywraca tylko start HMI; reszte cofa rollback.)
6. Zachowaj RATUNEK_LOG.txt z karty ratunkowej - pokaz mi go.

Jesli i to nie pomoze: pelne wgranie fabrycznej paczki K3346.


TWARDY RESTART MMI
------------------
Pewne (wiki M.I.B.): wyjecie bezpiecznika MMI albo odlaczenie akumulatora
na chwile.
Do sprawdzenia w Twoim aucie: przytrzymanie przycisku On/Off (galka
glosnosci) przez co najmniej 10 s, az pojawi sie ekran startowy - tak opisuje
to biuletyn serwisowy Audi dla nowszych MMI. Dla A4 B9 z MIB2 High nie
mam tego potwierdzonego - sprawdz podczas testu karty.


CZEGO TA KARTA NIE GWARANTUJE
-----------------------------
Test na zdrowym aucie dowodzi, ze autorun, tryb aktualizacji i skrypt dzialaja
w Twoim aucie z ta wersja M.I.B. Nie dowodzi, ze zadzialaja w trakcie bootloopu -
to potwierdzono tylko u luka-dev (inna platforma, MHI2Q). Argument za: autorun
i swdlclient sa natywne, a mod nie dotyka MIBRoot.

Co zmieniono w M.I.B. 3.6.0: tylko Launcher/stuff.sh (dopisane wywolanie
ratunek/ratunek.sh). metainfo2.txt (podpisany) i finalScript.sh (suma SHA1
w metainfo2.txt) - nietkniete. Dodane: ratunek/, RATUNEK_README.txt,
SHA256SUMS.txt. _Swdlautorun.txt przemianowany na Swdlautorun.txt.
