# Tangram w C++ – stan prac (etap 2)

Cel: nowa wersja gry napisana od zera w C++, czytająca dane (grafiki, muzykę, figury) z oryginalnego ADF.
Kod oryginału to skompilowany GFA-BASIC (TANCRUNCH.PRG), więc zasady trzeba odtwarzać z kodu maszynowego.

## Gotowe
- `src/adf.*` – czytnik obrazów ADF (OFS/FFS), listuje i wyciąga pliki.
- `src/image.*` – dekoder obrazów `.lo5` (320x200, 5 planów, plan po planie po 8000 B) + zapis PNG.
- `tools/dump_adf` – zrzuca wszystkie pliki z dyskietki.
- `tools/extract_pictures` – zapisuje ekran Thalion, tytułowy i wyników jako PNG.

## Etap 2: tabela 200 figur (GOTOWE)
Gra trzyma figury w tablicy GFA 54 x 200 słów (wiersz 0 = czas, wiersz 1 = liczba współrzędnych n,
wiersze 2.. = x0,y0,x1,y1... względem środka planszy). Ekran: x = 114 + x, y = 100 - y (czerwona figura);
cień to ta sama figura przesunięta o (+5,+5), rysowana na czarno.
- `tests/data/levels.bin` – tabela wyciągnięta z działającej gry (200 figur, format "TGLV"); to dane z Twojej dyskietki, tylko do użytku prywatnego.
- `src/levels.*` – wczytywanie tabeli, `src/raster.*` – wypełnianie wielokątów (reguła dopasowana do gry:
  IoU 0,993 z sylwetką poziomu 1 z prawdziwej gry, 44 piksele różnicy na 6560).
- `tools/render_levels` – rysuje wszystkie 200 figur na jednym obrazie.
- `tests/test_levels` – sprawdza tabelę i wypełnianie względem gry.
- Format samego `tangram.dat` (jak z niego powstaje ta tablica) jest jeszcze NIEROZPRACOWANY: plik ma 43 KB, a tablica
  tylko ok. 10 KB, więc DAT zawiera coś jeszcze (np. rozwiązania). Na razie figury bierzemy z `levels.bin`.

## Co wiemy o plikach
| Plik | Rozmiar | Zawartość |
|---|---|---|
| thalion.lo5 | 40064 | obraz 320x200 + paleta 32 kolorów (ostatnie 64 B) |
| titlepic.lo5, highscorepic.lo5 | 40000 | obrazy 320x200; paleta = pierwsze 32 słowa `tangram.mlu` |
| tangram.mlu | 62854 | zaczyna się od palet (słowa 0x0RGB), dalej grafika (figury, kursor, tła) |
| tangram.dat | 43498 | zaczyna się tekstem "84\n486\n", potem dane binarne – prawdopodobnie 200 figur |
| music1.img, music2.img | 31800 / 30266 | moduły muzyczne (format odtwarzacza do rozpoznania) |
| score.dta | 516 | tabela wyników: rekordy "napis(len)+punkty+poziom" |
| tancrunch.prg | 34344 | program GFA spakowany; po rozpakowaniu ok. 0x30000 B kodu |

## Zasady gry (z kodu i obserwacji)
- Figura do ułożenia to jeden wielokąt (ok. 20 wierzchołków), rysowany przez AreaMove/AreaDraw/AreaEnd;
  pierwszy etap: wierzchołki zaczynają się od (0x1D,0x57),(0x1D,0x8F),(0x39,0x73)...
- Siedem klocków, obrót lewym/prawym przyciskiem w krokach 22,5° (instrukcja), 16 pozycji.
- Zakrycie figury sprawdza się odczytem kolorów pikseli (ReadPixel).
- Klawisze: Spacja pauza, M muzyka, E koniec etapu.

## Do zrobienia (kolejność)
1. Format `tangram.dat` – odtworzyć jak powstaje tablica figur (mamy gotowe 200 figur jako wzorzec do testów).
2. Format `tangram.mlu` – wyciąć sprite'y i kursor.
3. Zasady: GOTOWE kształty 7 klocków i obrót (src/pieces.*, test na 272 wierzchołkach z gry); model planszy src/board.* (pokrycie/nakładanie/wystawanie, tolerancja jeszcze nieznana).
   Zostaje: dokładna reguła ukończenia etapu (rutyna w MLU pod 0x10d074), punktacja, czas, hasła (5 cyfr), tabela wyników.
4. Muzyka: odtwarzacz modułów (format z `music*.img`) + miksowanie 4 kanałów.
5. Okno i wejście: SDL2 lub Win32; plik wykonywalny dla Windows (potrzebny kompilator Windows – w tym środowisku go nie ma).

## Etap 4 – próba ustalenia reguły ukończenia etapu
- Ustalone: podczas układania i przy klawiszu E gra NIE wywołuje ReadPixel (0 wywołań w teście) – sprawdzenie
  robi kod maszynowy z tangram.mlu (rutyny skanujące, np. 0x10d33c/0x10d378, operują bezpośrednio na bitplanach, 5 płaszczyzn).
  Wcześniejsza notatka o ReadPixel była więc błędna.
- tools/solve_level: solver brute-force (piksele, 16 obrotów). Poziom 1: pole figury 6520 px; dla tolerancji 8 i 16 px
  brak rozwiązania (przy 30+ px bardzo wolno). Wniosek: klocki nie pokrywają figury co do piksela (zaokrąglenia na skosach),
  trzeba szukać rozwiązania na poziomie geometrii (wierzchołki), a nie pikseli.
- Następny pomysł: solver geometryczny (dopasowanie krawędzi/wierzchołków figury), potem test w symulatorze Go:
  ułożyć rozwiązanie skryptem myszy i sprawdzić flagę a5-$78fa; potem zaburzać pozycje o 1..N px, żeby zmierzyć tolerancję.

## Etap 5 – rozwiązanie poziomu 1 i pomiar tolerancji w prawdziwej grze
- tools/solve_level: po przejściu na rasteryzację "środek piksela w wielokącie" (bez nakładania na krawędziach)
  solver znajduje dokładne rozwiązanie poziomu 1 w 0,1 s (tests/data/level1_solution.txt: typ obrót x y).
- Rozwiązanie ułożone w symulatorze Go (skrypt myszy) => gra pokazuje "PASSWORD FOR LEVEL 002 03797": reguła ukończenia potwierdzona
  od końca do końca. Przesunięcie WSZYSTKICH klocków o 3 px => brak sukcesu.
- Pomiar: 91 przebiegów (jeden klocek przesunięty o 1..4 px w x/y), wynik w tests/data/level1_game_tolerance.txt.
  Gra toleruje ok. 1-2 px przesunięcia jednego klocka, 3+ px zwykle nie.
- Dopasowana reguła: niezakryte piksele figury <= 1% pola figury (solved()), zgodność z grą 81/91 (89%).
  Piksele poza figurą i nakładanie się klocków nie wchodzą do reguły. Dokładna rutyna (kod maszynowy MLU) nadal nierozczytana;
  rozbieżności wynikają zapewne z innej rasteryzacji w grze (Area* Amigi) niż w fillPolygon.
- Kolejne kroki: punktacja i czas (ekran: 059 = czas, dwa liczniki punktów), format hasła (5 cyfr, "03797" dla poziomu 2),
  tabela wyników, grafika z tangram.mlu, muzyka, okno SDL2/Win32.

## Etap 6 – punktacja, czas, hasła (obserwacje, jeszcze bez pełnego modelu)
Dane w tests/data/game_observations.txt (z prawdziwej gry, rozwiązania z solve_level; poziomy 1-3 rozwiązane łańcuchowo).
- Hasła (5 cyfr) po ukończeniu poziomu: L2=03797, L3=58829, L4=73159 – zależą od numeru poziomu, algorytm nieznany (3 próbki to za mało).
- Plansza: licznik czasu schodzi z 96 (poziom 1 ~100); drugi licznik ("bonus") zaczyna od 25*poziom (L2: 50, L3: 75, L4: 100)
  i rośnie w trakcie układania (zależy od ułożonych klocków, nie od czasu: ten sam wynik przy różnych opóźnieniach).
- Przyrost wyniku po ukończeniu: poziom 2: ok. czas_pozostały + bonus (+2..3), pokrywa się dla 4 próbek (57/70→130, 50/70→122, 44/70→116, 39/68→110);
  poziom 3: 55/95→153 (też ~ czas + bonus). Poziom 1 odbiega: czas/2 + bonus (59/34→65 … 32/34→51), prawdopodobnie inny mnożnik na 1. poziomie.
- Nie zaimplementowane w C++ – żeby nie zgadywać. Potrzebne: obserwacja zmiennych gry (a5-offsety) zamiast odczytu liczników ze zrzutów.

## Etap 7 – punktacja odczytana z pamięci gry (poprawka etapu 6)
Obserwacja zapisów do zmiennych gry (a5 = 0x117980; adresy = a5 + ofset):
- 0x11009d (bajt, a5-$78e3) = licznik upływu czasu E; rośnie o 1 co ~75 klatek (1,5 s) w trakcie gry (kod 0x102f44).
- 0x110036 (słowo, a5-$794a) = licznik premii; na starcie etapu = 25*poziom (0x102d5e); po rozwiązaniu pętla (0x103120..0x1032a0):
  dopóki E<101: E += (E==98 ? 1 : 2), premia += x/2+1, gdzie x = bajt 0x110082 (a5-$78fe) = 1,2,3 na poziomach 1-3.
  Potem premia jest odliczana do 0 krokami 1000/100/10/1 i dodawana do wyniku (licznik 0x12bf8a), co widać jako "drugi licznik".
- Wynik: przyrost = 25*poziom + n*(x/2+1); zgodne z 65 (L1,E=22), 130 (L2), 153 (L3). src/scoring.* + tests/test_scoring.cpp.
  Wyświetlany "czas" (059 itd.) to nie E wprost – wzór na jego wyświetlanie nie jest jeszcze ustalony.
- Niepewne: x dla poziomów >3 (przyjęto x = poziom); wartość E w momencie ukończenia przyjmowana z licznika klatek / 75.
- Hasło (5 cyfr) – nadal nieodczytane (próbki: L2 03797, L3 58829, L4 73159).

## Etap 8 – hasła
- Tabela haseł leży w pamięci gry zaraz po starcie jako 1000 cyfr (200 x 5), adres 0x139869 (sterta; wypełniana z tangram.dat
  przy starcie). Hasło poziomu N = wpis N (poziom 1 = 56885). Zgadza się z trzema hasłami pokazanymi przez grę (L2-L4).
- tests/data/passwords.txt, src/passwords.* + test. Tabela nie jest liczona wzorem – to dane z tangram.dat.
- Wniosek: poziomy (levels.bin) i hasła pochodzą z jednego pliku tangram.dat (43 KB, nagłówek "84\n486\n"); samodzielny port potrzebuje
  dekodera tego pliku (pod 0x1000xx rozpakowanie do tablic) – to kolejny krok, bo dziś levels.bin i passwords.txt są wyciągnięte z działającej gry.

## Etap 9 – poziomy i hasła czytane wprost z dyskietki (poprawka wcześniejszych wniosków)
- Wcześniejsza hipoteza o dekompresji tangram.dat była błędna dla tych danych: gra czyta poziomy i hasła z TANGRAM.MLU
  (uchwyt pliku z buforem 0x193ce8), a nie z TANGRAM.DAT, i to bez żadnej dekompresji.
- Tabela figur: surowa tablica 54 x 200 słów (big endian) od offsetu 64 w TANGRAM.MLU (wiersz 0 = czas, 1 = liczba wierzchołków,
  2.. = współrzędne; kolumna = poziom); gra czyta 21598 bajtów (o 2 mniej, ostatnie słowo = 0).
- Hasła: 1000 surowych bajtów 0-9 od offsetu 61662 w TANGRAM.MLU (5 na poziom).
- C++: parseLevelsFromMlu() i Passwords::loadFromMlu(); tests/test_from_adf.cpp porównuje z tabelami wyciągniętymi z działającej gry
  (0 różnic na 200 poziomach, hasła identyczne). Eksporter w Go nie jest już potrzebny.
- TANGRAM.DAT: patrz etap 12 (to archiwum obrazków, a nie dane poziomów).

## Etap 10 – tabela wyników (SCORE.DTA)
- Format (516 B): ciąg napisów [u16 BE długość][bajty]; 60 napisów = tabela A: 12 x (nazwa[8], wynik[7], poziom[3]) + tabela B: 12 x (nazwa[8], wynik[7]).
  Nazwy dopełniane spacjami do 8 znaków ("TANGRAM "), liczby wyrównane do prawej spacjami. Wyniki domyślne: 1200000, 1000000, 450000, 400000, 350000,
  100000, 50000, 20000, 15000, 10000, 5000, 200; poziomy tabeli A: 150,125,100,80,60,50,40,30,20,10,5,2.
- C++: src/highscores.* (parse, serialize – bajt w bajt jak oryginał, rankOf, insert) + tests/test_highscores.cpp, tests/data/score.dta.
- NIEZWERYFIKOWANE w grze: co oznaczają tabele A i B (poziom vs. bez poziomu; być może tryb 1/2 graczy albo "dziś"/"wszech czasów"),
  oraz reguła remisów (przyjęto: nowy wynik wpada poniżej równych) i sposób wpisywania imienia.

## Porównanie z listą cheatów od użytkownika (199 haseł, poziomy 2-200)
- tests/data/cheat_list_from_user.txt vs tabela z MLU: 184 zgodne (po pominięciu zer wiodących), 15 różni się o jedną cyfrę:
  L6 22585/22586, L8 50037/60037, L12 25105/26106, L15 85720/86720, L24 85239/86239, L25 72851/72861, L31 51522/61522,
  L32 58875/68875, L35 36388/36338, L43 50897/60897, L52 38599/38559, L58 91998/91988, L141 95862/95850, L142 95850/17617, L149 44059/44049.
- Wskazówki, że błąd jest w liście zewnętrznej: ma duplikat (25105 dla L12 i L170), a nasza tabela ma 200 unikatowych kodów; L141/L142 w liście wyglądają na przesunięte.
  Nasza tabela pochodzi z danych gry, a L2-L4 potwierdzone na ekranie gry. Do rozstrzygnięcia: wpisanie spornych kodów w grze (nie zrobione).
- ROZSTRZYGNIĘCIE W GRZE (symulator Go, wpisanie kodu w menu wyboru: klik w pole PASSWORD, 5 cyfr, RETURN): wszystkie 15 kodów z naszej tabeli
  zostało przyjętych; kody z listy zewnętrznej zostały odrzucone (gra startuje wtedy od poziomu 1) z dwoma wyjątkami, które są poprawnymi
  kodami innych poziomów (25105 = poziom 170, 95850 = poziom 141). Lista użytkownika ma więc literówki; tabela z TANGRAM.MLU jest poprawna.
  Uwaga: przy kodzie poziomu 8 (60037) ekran pokazał etykietę "009" przy bonusie 200 (=25*8) – niewyjaśnione, nie badane.

## Etap 11 – układ TANGRAM.MLU i obraz planszy
Układ pliku (62854 B), ustalony z kolejności odczytów gry:
  0..64 paleta ekranu tytułowego | 64..21662 tablica figur | 21662..61662 obraz planszy 320x200, 5 płaszczyzn po 8000 B (smok + tacka z klockami)
  | 61662..62662 hasła | 62662..62854 trzy palety po 32 kolory dla obrazu planszy (paleta 0 = zgodna z grą na poziomie 1; 1 i 2 inne odcienie).
- C++: src/assets.* (mluTitlePalette, mluBoardPalette, mluBoardPicture), tools/render_board, tests/test_assets.cpp:
  obraz zgodny z zrzutem ekranu gry (start poziomu 1) w 99,36% pikseli poza obszarami, na których gra rysuje własne elementy (figura, liczniki, pasek czasu).
- Nie ma jeszcze: sprite'ów klocków i kursora (klocki rysowane przez Area* wielokątami – patrz pieces.*), czcionki, ekranów menu, ramki hasła (rysowane kodem).
- Do ustalenia: skąd gra bierze kolory klocków/figury per poziom (palety 1 i 2 różnią się tylko kilkoma kolorami, np. kolor 14: c20 vs 820).

## Etap 12 – TANGRAM.DAT = archiwum obrazków (poprawka)
- Format: linia "84", potem dla każdego obrazka linia z długością i tyle bajtów: u16 głębia, u16 szerokość-1, u16 wysokość-1,
  potem `głębia` płaszczyzn po wysokość wierszy x ((szer+15)/16)*2 bajtów. 85 obrazków, wszystkie rekordy zgadzają się z tym wzorem co do bajtu.
- Zawartość (wg arkusza z tools/dump_dat): 0-3 napisy przycisków NOVICE/EXPERT/1PLAYER/2PLAYER (61x12), 4 panel menu (109x138, z polem PASSWORD),
  5-6 paski czcionki 296x8 (A-Z, 0-9, (c)) w dwóch kolorach, 7 i 84 panele 206x61 ("GAME PAUSED" i pusty na hasło/komunikaty), 8-9 YES/NO (27x10),
  10-12 kursory/ręka (20x24), 13-32 ikony 10x10 (klocki, liczniki), 33-82 sprite'y 16x16, 83 pasek czasu (52x8).
- Czcionka: gra buduje bitmapę 296x8 z rekordu 5 (ClipBlit, glify 8x7 wycinane co 8 px) – rekord 5 identyczny z pamięcią gry (0 różnic w teście).
- C++: src/dat.* (parseDat, DatPicture::toImage), tools/dump_dat (arkusz PNG), tests/test_dat.cpp.
- Kolejność glifów w pasku i dokładne przypisanie rekordów 13-82 do elementów gry: do ustalenia (następny krok).

## Etap 13 – czcionka i układ tekstów (z działającej gry)
- Gra buduje bitmapę 296x8 z rekordu 5 (lub 6) i wycina komórki 8x7: A-Z, 0-9, (c) (37 komórek; spacja bez glifu – kursor idzie o 8 px).
  Rysowanie znaku = ClipBlit temp 8x7 -> ekran; indeks koloru 0 jest przezroczysty (tło panelu zostaje).
- Rekord 5 = granatowe napisy (tytuł/credits), rekord 6 = napisy interfejsu gry (liczniki, czas, panel hasła).
- Pozycje tekstów (src/layout.hpp): wynik (234,119) 7 cyfr; premia (234,133) 7 cyfr; czas (263,174) 3 cyfry, od 100 w dół; etykieta poziomu (236,189);
  panel hasła: PASSWORD (26,91), FOR (98,91), LEVEL (130,91), numer (178,91), kod (94,103); credits: DESIGN/MUTSCHALL, MUSIC/HIPPEL, AMIGA VERSION/BEHRENDS/MAACK
  oraz "(c) 1991 BY MARK LUTHE" na ekranie startowym (y=2).
- C++: src/font.* (glyphCell, drawText), src/layout.hpp, tests/test_font.cpp: tekst panelu hasła wyrenderowany w C++ jest identyczny
  piksel w piksel (977/977) z zrzutem ekranu prawdziwej gry.
- Wyświetlany czas = 101 - E (E = licznik z etapu 7): wyjaśnia, dlaczego "059" na ekranie nie równało się wartości E.
- Nie ustalone: rola ikon 10x10 i sprite'ów 16x16 (rekordy 13-82) oraz kursorów (10-12).

## Etap 14 – obrazki z TANGRAM.DAT na ekranie (częściowo)
Metoda: dopasowanie szablonów (wszystkie 85 rekordów, 4 palety) do zrzutów ekranu z menu, startu planszy i trzymanego klocka.
Potwierdzone co do piksela (tests/test_sprites.cpp, 1439/1439 pikseli):
- rekord 0 (NOVICE) w (231,83) i rekord 2 (1PLAYER) w (231,116) na ekranie menu, paleta tytułowa (pierwsze 64 B MLU);
- rekord 11 (wskaźnik 20x24) rysowany w pozycji myszy + (-2,0), paleta 0 planszy.
- rekord 83 (pasek czasu 52x8) w (249,162) – dopasowanie 93% (pasek częściowo wypełniony), więc położenie pewne, szczegóły wypełniania nie.
- rekord 32 (10x10) znaleziony w (89,110) przy trzymanym klocku – rola niejasna.
Nie dopasowane: pozostałe przyciski (1 EXPERT, 3 2PLAYER – prawdopodobnie inny wygląd zaznaczenia), panel menu (4), panel PAUSED (7; klawisz spacji w symulatorze
nie wywołał pauzy na zrzucie), reszta ikon 10x10 / sprite'ów 16x16. Obserwacja: gra rysuje klocki wielokątami (Area*), a nie sprite'ami z DAT – dlatego
większości rekordów 13-82 nie widać na zrzutach z rozgrywki; mogą służyć do ekranów wyboru/podsumowania albo pozostać nieużywane.
- C++: src/sprite.* (blitPicture), stałe w src/layout.hpp, Rgb::operator==.
- Do zrobienia: zrzuty pozostałych stanów (menu z zaznaczeniem EXPERT/2PLAYER, pauza, tabela wyników, koniec gry), żeby dopasować resztę rekordów.

## Etap 15 – więcej obrazków dopasowanych do ekranów gry
Nowe zrzuty: menu z zaznaczonym EXPERT / 2PLAYER, pauza (klawisz spacji; dodany do skryptów symulatora), Esc w trakcie etapu.
- Esc w grze pokazuje panel "PLAY WITH SCOREDISK" z przyciskami YES / NO (pytanie o dyskietkę z wynikami); to ekran, na który wchodzi się przy kończeniu etapu klawiszem Esc.
- Potwierdzone pikselowo (12194/12194 + 1439/1439, tests/test_sprites.cpp): rekord 1 EXPERT (231,96); 7 "GAME PAUSED" (12,67); 8 YES (85,105); 9 NO (117,105);
  10 = wskaźnik na ekranie menu (paleta tytułowa, mysz + (-2,0)); 11 = wskaźnik w grze; 0 i 2 jak wcześniej.
- Zaznaczenie przycisków menu (jasna ramka wokół wybranej pozycji) nie wynika z innego rekordu: rysuje je kod gry.
- Nadal bez przypisania: rekord 3 (2PLAYER – zasłaniany wskaźnikiem), 4 (panel menu), 12 (trzeci wskaźnik), 13-31 (10x10), 33-82 (16x16), 83 (pasek czasu, tylko położenie), 84 (pusty panel – tekst "PLAY WITH SCOREDISK" i PAUSED nakładane na niego).

## Etap 16 – informacje od użytkownika o rekordach DAT (niezweryfikowane w grze)
- Rekordy 33–82 (50 sprite'ów 16×16) = ikony etapu dodatkowego: po każdym 10. poziomie gracz klika
  identyczne ikony (dodatkowe punkty). Wg użytkownika, nie z własnej obserwacji.
- Rekordy 13/14–32 (ikony 10×10): przeznaczenie nieznane (użytkownik też nie wie).
- Próba dojścia do etapu dodatkowego w symulatorze: wymaga rozwiązania poziomu 10 (indeks 9);
  solver (środki pikseli, tol 0–80) nie znajduje rozwiązania dla indeksów 3, 4, 9, 10 – figury
  prawdopodobnie nie leżą na siatce, którą zakłada DFS. Do zrobienia: lepszy solver albo
  podejrzenie zmiennej poziomu w pamięci gościa i skok do etapu dodatkowego.

## Etap 16b – solver poprawiony, poziomy 10 i 20 rozwiązane
- Przyczyna porażek solvera: pojedyncze „sieroty” pikseli na krawędziach figury (zaokrąglenia wierzchołków),
  których nie da się pokryć żadnym elementem. Dodano „zwolnienie” piksela (budżet luk, domyślnie 62 ≈ 1%):
  `solve_level levels.bin idx [tol] [gap]`; działa np. `tol=2 gap=20` (poziom 10: idx 9, poziom 20: idx 19).
- Rozwiązania w tests/data/level10_solution.txt i level20_solution.txt (typ, rot, cx, cy).
- Sprawdzone w symulatorze Go: start z hasła 07274, ułożenie poziomu 10 -> wynik 472 pkt, ekran
  „PASSWORD FOR LEVEL 011”, a po kliknięciu od razu poziom 11. Etapu dodatkowego NIE było.
  To samo po poziomie 20 (hasło 14278). Hipotezy: etap dodatkowy włącza się tylko przy kolejnym
  przejściu poziomów bez użycia hasła, albo liczy się inny licznik. Do wyjaśnienia.

## Etap 16c – zrzut etapu dodatkowego od użytkownika (YouTube, 686×386, skalowany; NIE z naszego symulatora)
- Plansza etapu dodatkowego to pole gry (lewa część, ok. 230 px szer.) wypełnione kafelkami 16×16
  (rekordy 33–82) ułożonymi „szachownicowo”: co drugie pole siatki zajęte, rząd nieparzysty przesunięty
  o pół kroku. Szacunek ze zrzutu (do potwierdzenia pikselowo): krok poziomy między kafelkami w rzędzie
  ≈ 36 px, przesunięcie rzędów ≈ 18 px, krok pionowy ≈ 20 px, ok. 10 rzędów. Kafelki mają czarny cień.
- Prawy panel jak w zwykłej grze: wyniki (0001392 / 0000040), pasek czasu + licznik „090”, elementy po prawej.
- Kafelki rekordów 33–82 = 10 kształtów × 5 wariantów kolorystycznych, po 5 kolejnych rekordów na kształt:
  (zielony, „złoty/inny”, fioletowy, czerwony, ciemny). Kształty: romb, kwadrat, trójkąt, koło,
  soczewka/liść, paski, linia ukośna, „X”, ramka, pierścień-romb/trójkąt… Wariant „złoty” ma inny wzór
  (paski, cztery kwadraty, kratka „II” itp.).
- Zaznaczenie kafelka (po kliknięciu) – czerwona ramka widoczna wokół czerwonego „X”; kursor to
  rekord DAT z kulą (jak w grze).
- Ikony 10×10 (13–32) NIE występują na tym zrzucie jako osobne elementy – pozostają niewyjaśnione.
- Brak: dokładne zasady (liczba par, kara za pomyłkę, punktacja, czas) i wyzwalacz etapu.

## Etap 17 – okno i pętla gry
- src/game.{hpp,cpp} (biblioteka tggame): klasa Game, niezależna od platformy. Wejście jako `Event`
  (ruch/klik myszy w pikselach 320×200, klawisze), `step()` = jedna klatka 50 Hz, `frame()` = obraz 320×200.
  Tryby: Menu (TITLEPIC.LO5 + panel rekord 4 w (211,62) + przyciski + kursor), Play, Password.
- Menu: cyfry + Enter = hasło (zły kod -> poziom 1), klik 1PLAYER = poziom 1. Esc w grze = menu, P = pauza.
- Gra: klik w tackę (pozycje z pomiaru) bierze element, PPM obraca o 22.5°, LPM kładzie (tylko na polu figury),
  klik w położony element podnosi go ponownie. Po każdym położeniu `measureCoverage` + `solved` (≤1% niepokrytych).
  Wygrana: wynik += stageGain(level, ticks), ekran „PASSWORD FOR LEVEL nnn” z kodem następnego poziomu.
- Okna: src/main_x11.cpp (Xlib, testowany tylko pod Xvfb: otwiera okno, renderuje, zapisuje PNG; wejścia
  myszy/klawiatury NIE testowane na prawdziwym oknie – brak xdotool) i src/main_win32.cpp (Win32/GDI,
  NIE kompilowany ani uruchamiany, brak MinGW). CMake: cel `tangram` (WIN32) lub `tangram_x11`.
- Zweryfikowane testem (test_game): menu zgodne z zrzutem w 96,3% pikseli; liczniki panelu (wynik, bonus,
  czas 095, poziom) pikselowo identyczne z prawdziwą grą; poziomy 1 i 10 rozwiązane zdarzeniami; hasła.
- Przybliżenia (nie z oryginału): kolory elementów (fioletowe) i obwódka; brak cyfr liczników na tacy,
  paska czasu (wypełnienia), muzyki, dźwięku, 2 graczy, etapu dodatkowego, tabeli wyników; paleta planszy
  zawsze 0; pole hasła w menu (245,150) przybliżone; etykiety 2PLAYER/PASSWORD w menu niedorysowane.
- Odkrycie: czcionka UI (rekord 6) jest rysowana „nieprzezroczyście” – indeks 0 = czarny tusz, reszta to tło
  pergaminu (drawText(..., opaque=true)).

## Etap 18 – dokończenie ekranu gry (dopasowane do zrzutów prawdziwej gry)
- Elementy: płaskie kolory, 7 egzemplarzy = rampa (0,0,68) (34,34,102) (68,34,136) (102,68,170) (136,102,204)
  (170,136,238) (204,170,238) w kolejności: duży#1, duży#2, kwadrat, średni, mały#1, mały#2, równoległobok.
  Kolor zależy od egzemplarza (nie od kolejności kładzenia); cień czarny +5,+5 rysowany pod każdym elementem
  zaraz przed nim; sylweta figury ma własny cień.
- Cyfry liczników na tacy (pozycje pikselowo dokładne, czcionka UI nieprzezroczysta): (244,25) duży,
  (268,49) kwadrat, (295,49) średni, (272,95) mały, (292,95) równoległobok; pokazują ile zostało
  (element trzymany w ręku liczy się jako zdjęty z tacy).
- Pasek czasu (rekord 83, rysowany nieprzezroczyście): zostaje lewych 52 − (E/2 + 1) kolumn, E = tiki+1;
  reszta to tło planszy (dopasowane do 4 zrzutów).
- Numer gracza „1” w (310,189) po wypalonym w obrazie napisie PLAYER.
- Menu: wybrany przycisk = pełny obrazek (turkusowe tło), niewybrany = tylko czarny napis (indeks 0): NOVICE/EXPERT i
  1PLAYER/2PLAYER; 2PLAYER w (231,129) pikselowo. Zostaje nierozwiązany napis PASSWORD (nie pochodzi z rekordów 5/6).
- Wynik: ramka gry z 6 położonymi elementami zgodna z prawdziwą grą w 99,82% pikseli (zostają: cień kursora
  i drobne różnice przy krawędzi sylwety); menu 98,0%.

## Etap 19 – ekran startowy z intro
- Kolejność (jak w prawdziwej grze, zrzuty z symulatora Go): logo Thalion (THALION.LO5, paleta w ostatnich 64 B pliku) →
  obraz tytułowy TITLEPIC.LO5 z napisem „© 1991 BY MARK LUTHE” → ten sam obraz z zwojem (rekord 4) i napisami
  (DESIGN MUTSCHALL / MUSIC HIPPEL / AMIGA VERSION / BEHRENDS / MAACK; czcionka rekord 5) → menu (zanikanie).
  Każdy obraz: wejście 25 klatek, trzymanie 220/220/225 klatek (czasy z wersji Go), wyjście 25 klatek (przyciemnianie
  palety przez skalowanie RGB). Kliknięcie pomija obraz, Enter/Esc – całe intro.
- Zgodność z prawdziwą grą (test_game): logo 100,00%, tytuł 99,61%, napisy 99,01% pikseli (przy pełnej jasności).
- Odkrycia: (1) czcionka rekord 5 – tylko indeks 0 jest tuszem (czarnym), reszta komórki przezroczysta; rekord 6 –
  komórka nieprzezroczysta (tło pergaminu). (2) Napis „© 1991 BY MARK LUTHE” nie pasuje do komórek czcionek –
  zapisany jako bitmapa 1-bitowa odczytana ze zrzutu (src/copyright.hpp). (3) Na ekranie napisów ramki przycisków zwoju
  są zakryte teksturą pergaminu, której nie ma w parsowanych plikach – załączona jako src/panel_texture.hpp
  (odczytana ze zrzutu). W menu ramki są widoczne (rekord 4 bez zmian).
- Menu jak w oryginale: lewy klik na smoku (lub prawy przycisk) startuje grę, przyciski tylko wybierają
  NOVICE/EXPERT i 1PLAYER/2PLAYER.
- Brak: muzyka w intro, przyciemnianie dokładnie jak w oryginale (liniowe 25 klatek – przybliżenie), napis PASSWORD w menu,
  kursor w intro (oryginał pokazuje mały kursor w lewym górnym rogu).

## Etap 20 – muzyka (oryginalny odtwarzacz Hippela uruchamiany na interpreterze 68000)
- music1.img / music2.img to PROGRAMY 68000 (odtwarzacz + utwór + próbki), nie dane modułu: wejście +0 = start (instaluje
  przerwanie CIA-A 50 Hz przez ciaa.resource, AddICRVector), +4 = stop. Utwór 1: logo, intro, menu; utwór 2: gra (Go: Music2
  ładowany po Highscorepic.lo5 przy starcie gry – kolejność przypisania do ekranów wnioskowana, nie sprawdzona dla ekranu wyników).
- src/m68k.{hpp,cpp}: interpreter 68000 przetłumaczony 1:1 z cpu.go (wraz z jego uproszczeniami). src/music.{hpp,cpp}: pamięć 16 MB,
  obsługa exec (OpenResource, Forbid/Permit…) i cia.resource, model Paula (4 kanały, DMACON, okres ≥124, głośność ≤64, kanały
  0,3 → lewy, 1,2 → prawy, filtr 0,5) i licznika CIA (przerwanie co (latch+1)/709379 s); czas wirtualny 1,7 µs/rozkaz (VHPOSR).
  Odkrycie: DMACON musi mieć ustawiony bit DMAEN (0x200) jak po starcie systemu, inaczej kanały milczą.
- Weryfikacja (test_music): 4306 zapisów do rejestrów Paula/CIA w 6 s utworu 1 = IDENTYCZNE z zapisami silnika Go grającego
  prawdziwą grę (złoty plik tests/data/music1_writes_go.txt, wskaźniki próbek względem początku obrazu). Utwór 2: działa 6 s bez błędu,
  niecichy i deterministyczny, ale bez złotego pliku. Próbki audio nie były porównywane z WAV z Go (inny moment zapisów względem
  próbek w obrębie klatki – różnica poniżej 20 ms).
- Game::audio(out, frames) renderuje bieżący utwór (1: intro+menu, 2: gra/hasło); klawisz M: głośność 25→50→75→100%→wył. (jak w Go,
  start 25%). Okna: X11 `--wav out.wav` / `--audio-stdout` (do aplay), Win32 waveOut (4 bufory × 40 ms, osobny wątek) – NIE testowane.

## Etap 20b – korekta: który utwór gra kiedy
- Użytkownik zauważył, że w grze C++ podczas rozgrywki leciała muzyka z etapu dodatkowego (utwór 2). Sprawdzenie w silniku Go: przy starcie
  gry NIE ma nowego AddICRVector ani zmiany utworu; strumień zapisów do Paula/CIA z ~115 s działania (menu + rozgrywka) jest identyczny z
  utworem 1 (44244 pierwszych zapisów = 60 s utworu 1 bez różnic). Wniosek: utwór 1 gra nieprzerwanie od logo przez menu i wszystkie poziomy.
  Utwór 2 (Music2.img) oryginał ładuje przy starcie, ale nie gra go w menu ani w poziomach – zgodnie z tym, co mówi użytkownik, należy
  do etapu dodatkowego. Game::wantedSong() zwraca więc zawsze 1; utwór 2 czeka na implementację etapu dodatkowego.

## Etap 21 - przejscie po ukonczeniu etapu (zaluzje)
Zmierzone na oryginale (sim Go, zrzuty co klatke, przeliczone na 50 Hz po ostatnim klocku):
- ok. 156 klatek: odliczanie bonusu (czas i bonus spadaja, wynik rosnie) - w C++ przyblizone liniowo (1 krok czasu na klatke);
- od klatki 162: panel z haslem odslania sie jak pionowe zaluzje: listwy co 16 px, srodki na granicy kolumn x = 24 + 16k,
  szerokosc 2,4,...,16 px, kazdy krok 6 klatek (pelny panel po ~48 klatkach). Dopasowanie modelu do zrzutow: 0 bledow kolumn;
- klik / Enter (po pelnym odslonieciu): panel zwija sie tak samo, ale listwy panelu maleja wokol x = 16 + 16k
  (szerokosc 14,12,...,0, co 6 klatek), a w szczelinach widac juz nastepny etap; potem startLevel(level+1) (po ostatnim etapie menu).
- NIE odtworzone / niepewne: dokladny rytm odliczania bonusu; przesuniecie "od prawej do lewej" z opisu uzytkownika nie wyszlo
  w pomiarze (zaluzje zamykaja sie rownomiernie na calej szerokosci) - byc moze to wrazenie z animacji wokol listew.
Test: tests/test_game.cpp porownuje prostokat panelu z prawdziwymi klatkami (blinds_open_*.rgb): 94-99% identycznych pikseli.

## Etap 22 - etap dodatkowy (PRZYBLIŻENIE)
Po każdym 10. etapie (Password -> kliknięcie) startuje Mode::Bonus: siatka 8x5 ikon 16x16 (rekordy DAT 33..82), 20 par
losowanych deterministycznie, muzyka nr 2. Klik dwóch identycznych ikon usuwa je (+50 pkt); koniec po usunięciu wszystkich par
albo po 1500 klatkach (30 s), potem następny poziom. Układ, punktacja, czas i zasady NIE pochodzą z oryginału (nieznane) -
to grywalna rekonstrukcja z opisu użytkownika. Do poprawy po porównaniu z prawdziwym ekranem.

## Poprawka - cienie klocków
Klocki NIE rzucają cienia (to był mój błąd); cień ma tylko sylwetka figury. Zgodność klatki z 6 klockami z prawdziwą grą: 99.86% (z cieniami klocków było 99.82%).

## Poprawka - przeciąganie klocków
Lewy przycisk w dół podnosi klocek (z tacki lub z planszy), puszczenie go odkłada (tylko nad polem figury; puszczenie nad tacką zostawia klocek w ręce,
więc "klik-klik" z symulatora też działa). Prawy przycisk obraca trzymany klocek. Test: przeciągnięcie klocka w tests/test_game.cpp.

## Poprawka - przeciąganie dla każdego klocka, okno w centrum
Puszczenie LPM poza polem figury (np. nad tacką) odsyła klocek na tackę - klocek nigdy nie zostaje "przyklejony" do kursora
(wcześniej puszczenie nad tacką zostawiało go w ręce, stąd klik-klik). Win32: SetCapture/ReleaseCapture (puszczenie poza oknem też dociera).
Okno startuje wyśrodkowane (Win32: GetSystemMetrics, X11: DisplayWidth/Height) - NIE sprawdzone na Windowsie.

## Poprawka - resztki klocków i kursor przy haśle
Listwy żaluzji to pasy na całej wysokości obszaru figury (wiersze 62..143): w otwartej listwie widać czyste tło + panel, więc po figurze
nie zostają fragmenty klocków poza prostokątem panelu (dopasowanie kolumn do oryginału bez błędów). Zamykanie używa tych samych pasów.
Kursor myszy jest ukryty od ukończenia etapu (odliczanie, hasło, zamykanie) aż do pokazania następnego etapu.

## Poprawka - napis PASSWORD w menu
Napis PASSWORD (tylko czarny tusz czcionki 6, pozycja {232,151}, dopasowana do zrzutu menu: 97.5% -> 97.9%) jest rysowany zawsze,
a wpisywany kod pojawia się pod nim ({243,164} - pozycja kodu oszacowana, bo na zrzucie go nie ma).

## Poprawka - czytelność napisu PASSWORD
Litery oryginalnego napisu są węższe niż czcionki z DAT (odstęp 7 px), więc napis jest teraz bitmapą wyciętą ze zrzutu menu
(src/password_label.hpp) - zgodność menu 97.9% -> 98.16%. Wpisywany kod: czcionka rekordu 5, wyśrodkowany pod napisem ({241,164}).

## Muzyka (głośność, F1-F10) i hasła
- Głośność startowa 25%; M wycisza, kolejne M przywraca 25% (zamiast cyklu 25/50/75/100/off).
- F1..F10 (wersja z crackiem): F1-F5 = muzyka normalnych etapów (utwór 1), F6-F10 = muzyka etapu dodatkowego (utwór 2). Podział jest MOJĄ decyzją -
  w symulatorze oryginału wszystkie klawisze F zadziałały tak samo, więc mapowania nie dało się odczytać. Jeśli ma być inne, zmiana w Game::event.
  Wybór zostaje do następnego wciśnięcia F (w etapie dodatkowym też obowiązuje). Win32: F10 przychodzi jako WM_SYSKEYDOWN - obsłużone.
- Hasła: wszystkie 200 kodów są w tabeli gry (TANGRAM.MLU) i działają (test sprawdza każdy). Lista od użytkownika różni się w 15 pozycjach jedną cyfrą
  (etapy 6, 8, 12, 15, 24, 25, 31, 32, 35, 43, 52, 58, 141, 142, 149) - te kody są akceptowane jako alternatywy, z wyjątkiem dwóch (12: 25105, 142: 95850), które są prawdziwymi kodami etapów 170 i 141 - tam wygrywa tabela gry.
- Wpisywany kod w menu: lewy brzeg x=242, y=162, odstęp 6 px (dopasowanie do zrzutu: ~60 pikseli różnic - cyfry oryginału są nieco inne niż w czcionce DAT).

## Esc i F2
Esc na ekranie menu (wybór poziomu / liczby graczy) zamyka grę (Game::quitRequested; X11: wyjście z pętli, Win32: DestroyWindow w WM_TIMER).
Zmiana utworu tylko klawiszem F2, cyklicznie: utwór 1 <-> 2 (F1, F3..F10 nic nie robią; wcześniejszy podział F1-F5 / F6-F10 usunięty).

## Wpisywane hasło w menu - dokładne dopasowanie
Poprzednie dopasowanie (odstęp 6 px) wynikało z błędnego zrzutu: klawisze w symulatorze wpisywałem zbyt szybko (w 10), więc cyfry były
przycięte. Przy w 15 między klawiszami: czcionka rekordu 5, pierwsza cyfra w {242,162}, odstęp 8 px = 0 różnic z prawdziwym zrzutem (kod 12345).
(Wcześniejsza notka o ~60 pikselach różnic jest nieaktualna.)
Poprawka: cyfry były rysowane samym czarnym tuszem (indeks 0), a w oryginale komórki rekordu 5 niosą też odcienie cieni, więc wyglądały na cienkie.
Teraz komórki są rysowane w całości (nieprzezroczyście) - prostokąt cyfr jest identyczny ze zrzutem prawdziwej gry (0 różnic dla kodu 12345).

## Hasła - kody bez wiodących zer
Lista z Lemon Amiga (tests/data/tangram_cheat_lemon_amiga.pdf, identyczna z listą użytkownika) podaje kody bez wiodących zer (etap 2: 3797,
etap 10: 7274, 51: 8439, 60: 2675, 63: 2353, 75: 5111, 89: 16, 101: 6764, 116: 7464, 118: 3695, 146: 1849, 181: 1657, 185: 2971, 187: 442, 197: 6).
Gra szukała dokładnie 5 cyfr, więc takie kody kończyły się etapem 1. Teraz kod krótszy niż 5 cyfr jest uzupełniany zerami z lewej (levelFor),
a test sprawdza wszystkie 199 kodów z listy tak, jak są wydrukowane. Dwa kody z listy (etap 12: 25105, etap 142: 95850) są w tabeli gry kodami etapów 170 i 141 - tam wygrywa tabela.

## Dane wkompilowane (bez .adf)
Narzędzie tools/pack_assets czyta .adf i zapisuje src/embedded_assets.cpp (tablice bajtów): TANGRAM.DAT, TANGRAM.MLU, TITLEPIC.LO5,
THALION.LO5, MUSIC1.IMG, MUSIC2.IMG (~250 KB; plik źródłowy ~720 KB). Game::init dostaje Adf wypełniony przez loadEmbeddedAssets (Adf::addFile).
tangram_x11 / tangram.exe uruchamiają się bez argumentów; ścieżka do .adf nadal działa jako nadpisanie. test_embedded: gra startuje bez .adf,
a z TANGRAM_ADF pliki są bajt w bajt takie jak w obrazie. Pozostałe testy nadal korzystają z .adf (wzorce zrzutów ekranu).

## Etap dodatkowy - według zrzutu i opisu z oryginału (zastępuje przybliżenie z etapu 22)
- Plansza: 50 kafelków 16x16 (sprajty DAT 33..82) na przekątnej szachownicy o komórce 18 px: 10 rzędów x 5, kafelek (rząd r, j=2k+(r&1)) ma lewy górny róg
  (25+18j, 11+18r) - dopasowane do zrzutu z oryginału (±1 px). Czarny cień kafelka +4,+4 (z oka, możliwe +5).
- 25 par; rodzaje sprajtów losowane ze zwracaniem (na zrzucie ten sam wzór występuje 4-5 razy).
- LPM zaznacza kafelek (czerwono-pomarańczowa ramka 1 px, kolor 170,51,17); drugi identyczny: oba znikają, +20 pkt w polu bonusu (pod wynikiem);
  inny kafelek: ramka znika i nic więcej; ponowne kliknięcie zaznaczonego: odznaczenie.
- Czas 100 s (licznik 100->0 co 50 klatek, pasek czasu proporcjonalnie); po ukończeniu albo po czasie: ściemnienie do czerni (25 klatek) i następny etap;
  punkty bonusu dodawane do wyniku na końcu (moje założenie - nie widać w opisie kiedy).
- Przed etapem dodatkowym: po kliknięciu ekranu z hasłem po etapie 10, 20, ... ekran ściemnia się do czerni i od razu pojawia się etap dodatkowy.
- Nie wiem: dokładna wielkość cienia, czas fade (25 klatek to założenie), czy jest bonus za pozostały czas, ile par daje "komplet", dźwięki.
- Test: test_game sprawdza pary, pomyłkę, +20/500 pkt, koniec i limit czasu.

## Etap dodatkowy - punkty za czas (według opisu użytkownika)
20 pkt za parę (25 par = 500). Po ukończeniu (albo po czasie): pozostałe sekundy zamieniają się w punkty bonusu (1 pkt za sekundę; licznik czasu
spada, licznik bonusu rośnie - 1 na klatkę), potem licznik bonusu jest dodawany do wyniku i zerowany (5 pkt na klatkę), na końcu ściemnienie
do czerni i następny etap. Tempo obu przeliczeń jest MOJE (nie zmierzone). Komplet w czasie bez straty sekundy = 500 + 100 = 600 pkt (test).

## Etap dodatkowy - losowe rozmieszczenie ikon (poprawka według użytkownika)
Ikony nie leżą na siatce: 50 kafelków jest rozrzuconych losowo po planszy (lewy górny róg x 25..187, y 11..173; odstęp co najmniej 17 px w osi X lub Y,
czyli bez dotykania i zasłaniania), przy każdym wejściu inaczej (std::random_device). Losowanie 200 razy: zawsze udaje się upakować (zapasowa siatka nie była potrzebna).
Układ ze zrzutu (szachownica 18 px) był przypadkiem jednego losowania. Zakres rozrzutu to moje oszacowanie ze zrzutu.

## Etap 23 – tabela wyników na końcu gry
- Esc w trakcie etapu → panel „PLAY WITH SCOREDISK” (YES / NO; rekordy DAT 8 i 9 rysowane w całości). Panel zgodny z oryginałem w 99,2% (prostokąt z napisem i przyciskami; cień pod panelem nie jest odtworzony).
- Podczas pauzy / pytania figura jest ukryta.
- Ekran tabeli: paleta planszy nr 1, obraz HIGHSCOREPIC.LO5, czcionka rekordu „light”; zgodny z oryginałem w 100% pikseli (tests/data/screen_highscores.rgb).
- YES: wynik może wejść do tabeli (wpisanie imienia do 8 znaków: litery, cyfry, spacja; Enter zatwierdza). NO: tabela jest tylko pokazana. (Ukończenie etapu 200: patrz Etap 27 – gra NIE kończy się, przechodzi w losowe etapy EXP.)
- Tabela jest pokazywana ok. 6 s (300 klatek, wartość oszacowana); kliknięcie pomija. Potem powrót do menu.
- „Scoredisk” = plik `tangram.sco` w bieżącym katalogu (zapis przy zmianie tabeli, odczyt przy starcie).
- Nieodtworzone: ścieżka „CREATE NEW SCORE DISK”, tabela B (bez poziomu), dokładny wygląd ekranu wpisywania imienia.

## Etap 24 – dokładny przebieg końca gry (sprawdzone w symulatorze oryginału)
Symulator dostał podmienialną „dyskietkę wyników” (zmienna TGSAVE = katalog z własnym SCORE.DTA, ma pierwszeństwo przed obrazem ADF), dzięki temu udało się
obejrzeć wpisywanie imienia. Wnioski:
- Esc w etapie ALBO upływ czasu (000) → panel „PLAY WITH SCOREDISK”. Na tym panelu Esc/Return nie robią nic (nie ma powrotu do etapu). Po upływie czasu licznik
  bonusu spada do 0 (wynik zostaje), po Esc bonus pozostaje.
- NO → od razu tabela (bez wpisywania). YES → „CREATE NEW SCORE DISK” (te same przyciski). Na nim NO → odczyt DF0:SCORE.DTA i tabela (z wpisywaniem, jeśli wynik się łapie);
  YES → „INSERT FORMATTED DISK”: tu YES tworzy nowy plik (zapis tabeli domyślnej) i pokazuje tabelę, NO / Esc / Return nie robią NIC (potwierdzone, ekran zostaje).
- Tabela A = tryb Novice (NAME / LEVEL / POINTS), tabela B = tryb Expert (NAME / POINTS, bez poziomu; nagłówek LEVEL zastąpiony teksturą – 34x10 px z zrzutu, src/hs_levelcover.hpp).
- Wpisywanie imienia: w wierszu tabeli, bez kursora i bez limitu czasu; do 8 znaków, litery zamieniane na wielkie, przyjmowane są też spacja, cyfry i znaki interpunkcyjne;
  Backspace kasuje; Return zatwierdza (pusta nazwa dozwolona). Imię zapisywane jest tak, jak wpisane (BEZ dopełnienia spacjami do 8 znaków).
- Remis: nowy wynik trafia NAD równy wpis (>=). Tabela A zapisuje też poziom, na którym skończono grę.
- Czasy (50 Hz od kliknięcia): czerń ok. 46 klatek (wczytywanie obrazka), tabela do ok. 298. klatki, kliknięcie ją pomija; po Return / kliknięciu tabela gaśnie w ok. 8 klatek,
  ok. 14 klatek czerni, menu pojawia się szybkim rozjaśnieniem.
- Esc w etapie bonusowym: w oryginale nie sprawdzone (nie dało się dojść do etapu 10 w symulatorze); zrobione tak samo jak w zwykłym etapie (panel „PLAY WITH SCOREDISK”).
- Testy: ekrany wpisywania imienia (puste, ABCDEFGH, znaki specjalne, tabela B) porównane z zrzutami z prawdziwej gry: 100% / 100% / 99,94% / 100% pikseli
  (znak '@' w oryginale to pełny czarny kwadrat 8x7 – od etapu 37 też u nas, 100%).
- Poprawka: trzymany klocek rysowany jest nad napisami panelu (wcześniej napisy prześwitywały przez klocek nad prawym polem).

## Etap 25 – tryb 2 graczy (zbadany w symulatorze)
- Gracze grają NA PRZEMIAN po jednym ukończonym etapie na wspólnej sekwencji: P1 etap n, P2 etap n+1, P1 etap n+2… (po etapie 1 gracza 1 gra gracz 2 etap „002PLAYER2”).
  Każdy ma własny wynik; panel pokazuje wynik bieżącego gracza i napis PLAYER1/PLAYER2 (zrzut z gry: 99,5% pikseli licznika/etykiety).
- Po etapie 10 w trybie 2 graczy NIE ma etapu bonusowego (P1 kończy 10 → hasło na 011 → P2 gra 11).
- Koniec gry (Esc albo upływ czasu) działa jak w 1 graczu, do tabeli trafia wynik bieżącego gracza (po etapie 1 P1 z 65 pkt, na etapie P2 do tabeli szło 0, nie 65).
- Nie sprawdzone: czy P2 po ukończeniu etapu 20 dostaje bonus; kto dostaje etap przy starcie z hasła (przyjęto: P1).

## Etap 26 – tryb Expert (zbadany w symulatorze)
- Expert = LOSOWY etap z 200 (zamiast kolejnych); figura i premia (25 x numer etapu) jak dla tego etapu, ale w panelu zamiast numeru etapu napis „EXP”
  (zgodność z zrzutem prawdziwej gry: figura 99%, liczniki 100%). Losowanie w oryginale zależy od czasu (stąd różne etapy przy różnych opóźnieniach);
  przyjęto rozkład równomierny 1..200.
- Po ułożeniu figury: odliczanie premii jak w Novice (156 klatek), BEZ panelu z hasłem i BEZ klikania. Po 225 klatkach od ostatniego klocka kolejny losowy etap
  „wjeżdża” od lewej krawędzi (x = 22 + 7/15 px na klatkę, 300 klatek; stara figura znika na końcu). Zgodność z klatkami z gry: 99,9% (t=100) i 99,1% (t=244).
- Hasła w trybie Expert nie są używane (start z hasłem w Expert daje losowy etap – nie sprawdzone w oryginale).
- Niepewne: animacja odświeżania panelu na początku przejścia (liczby klocków wracają po jednej, pasek czasu rośnie) – u nas panel jest od razu nowy;
  moment startu zegara (przyjęto: po zakończeniu przejścia); wynik za etap w Expert wyszedł w symulatorze 52 zamiast 55 (Novice) przy podobnym czasie – nie wyjaśnione.

- Poprawka (wszystkie etapy): żaluzje obejmują tylko wiersze 62–143, a wyższe figury (np. etapy 10, 20) wystawały poza pasek – resztki klocków zostawały po otwarciu panelu z hasłem. Teraz poza paskiem tło jest czyste od razu po otwarciu panelu. Test: etapy 10 i 20 (tests/test_game.cpp).

## Korekta trybu 2 graczy (na polecenie użytkownika)
- Etap 25 opisywał obserwację z symulatora (gracz 2 od razu na etapie n+1). Użytkownik, znający oryginał, wskazał, że to błąd: obaj gracze grają KAŻDY etap.
  Obowiązuje: P1 gra etap n → odliczanie premii i (bez panelu z hasłem, bez klikania) „wjazd” tego samego etapu od lewej dla P2 → P2 gra etap n →
  panel z hasłem do n+1 i kliknięcie → P1 gra etap n+1. Wyniki osobne, bez etapu bonusowego w 2 graczy.
- Sprzeczność z symulatorem (tam po etapie 1 gracz 2 miał etykietę 002) nie została wyjaśniona; przyjęto wersję użytkownika.
  Przejście P1→P2 nie zostało porównane z oryginałem (użyto tego samego efektu co w Expert).

- Poprawka: klocki z prawego panelu można chwycić w całym widocznym obrysie (wcześniej tylko ±16 px od środka – romb nie dawał się chwycić za dolną część). Prostokąty chwytania: kTrayBox w src/game.cpp, test w tests/test_game.cpp.
- Poprawka 2: równoległobok (klocek z prawej kolumny panelu) sięga w dół do y≈137 – prostokąt chwytania wydłużony ({294,88,315,140}).

## F7 – rozmiar okna
- F7 zmienia rozmiar okna cyklicznie: 960x600 (3x), 1280x800 (4x)… aż do największej wielokrotności 320x200, która mieści się na ekranie (640x400 pominięte na życzenie; tylko gdy ekran nie mieści 3x, najmniejsze jest 2x)
  (Windows: obszar roboczy z uwzględnieniem ramki okna; Linux/X11: ekran minus 80 px na dekoracje), potem wraca do 640x400. Okno jest za każdym razem centrowane.
- Tylko warstwy okna (main_x11.cpp, main_win32.cpp); gra nadal liczy w 320x200. F7 i F8 nie trafiają do gry.
- Sprawdzone tylko na Linuksie (Xvfb 1920x1080: 960x600 → 1280x800 → 1600x1000 → 640x400 …). Wersja Win32 nieskompilowana.

## F8 – filtr obrazu
- F8 przełącza cyklicznie: wyłączony (najbliższy sąsiad) → ostrzejszy (reguła EPX/Scale2x uogólniona na dowolny współczynnik: tylko narożne ćwiartki piksela mogą przejąć kolor sąsiada, płaskie obszary i rastrowanie zostają) → wygładzony (bilinearny między środkami pikseli).
- Kod wspólny: src/scale_filter.{hpp,cpp}, test tests/test_scale.cpp. Filtr działa tylko na powiększeniu okna; ramka gry 320x200 i testy pikselowe bez zmian.
- Win32: bufor (320n)x(200n) wysyłany 1:1 przez StretchDIBits (nieprzetestowane na Windows).

## Ikona
- assets/icon.png (wycięte koło ze smoka od użytkownika), assets/tangram.ico (16–256 px), assets/tangram.rc; Win32: wc.hIcon z zasobu 1; X11: _NET_WM_ICON 64x64 z src/icon_data.hpp.
- Nie udało się zbudować tangram.exe w środowisku: brak kompilatora/SDK Windows (mingw, MSVC) i dostępu do sieci. Build: build_windows.bat na Windows.

## Etap 27 – po etapie 200 (symulator Go) i szybszy solver
- tools/solve_fast.cpp: szybszy solver (wskaźnik pierwszego wolnego piksela, odcięcie po polu, odrzucanie kandydatów wystających przed pierwszy wolny piksel, duże klocki najpierw); etap 20: 9 s zamiast 94 s. Opcjonalnie SOLVE_HINTS=plik ("typ rot cx cy") + SOLVE_R=promień ogranicza kandydatów do okolicy przybliżonego ułożenia (tak dokładnie ułożono etap 200 z przybliżenia odczytanego ze zrzutu użytkownika: 2,5 s). Rozwiązania: tests/data/level199_solution.txt, level200_solution.txt.
- Obserwacja w oryginale: po ukończeniu etapu 200 (Novice) gra się NIE kończy i nie pokazuje panelu z hasłem: bonus przelewa się do wyniku, panel się odświeża, a nowa figura wjeżdża od lewej jak w trybie Expert – etykieta „EXP”, losowy etap, wynik liczy się dalej. Port: flaga beyond_ (wipeMode, randomLevel, etykieta EXP od początku wycierania); tabela wyników pozostaje A.
- Kod etapu 200 (95039) jest w oryginale ODRZUCANY (start od etapu 1), a kod 75258 (etap 199) działa; po ukończeniu 199 panel pokazuje „PASSWORD FOR LEVEL 200 95039”. Port odrzuca ten kod tak samo.
- (Różnica opóźnienia panelu z tego wpisu poprawiona w Etapie 29.)

## Etap 28 – 2 graczy: korekta korekty
- Użytkownik (po sprawdzeniu oryginału): gracz 1 zaczyna od etapu 001, a gdy zaczyna gracz 2, jest już etap 002 – gracze grają NASTĘPNE etapy na przemian (zgodne z wcześniejszą obserwacją w symulatorze). Poprzednia wersja „obaj grają ten sam etap” (handover_) usunięta; po każdym etapie panel z hasłem następnego, brak etapu dodatkowego w trybie 2 graczy bez zmian.
- Esc w etapie dodatkowym: w symulatorze oryginału po etapie 10 etap dodatkowy się nie pojawił (od razu etap 11), więc nie do zweryfikowania; zostaje jak w zwykłym etapie.

## Start gry z menu
- Tylko Enter lub prawy przycisk myszy (na użytkownika: lewy klik na smoku usunięty; lewy klik nadal wybiera przyciski NOVICE/EXPERT/1PLAYER/2PLAYER).

## Etap 29 – odświeżanie panelu w trybie Expert (dane z symulatora)
- Zdjęcia co 8 klatek przejścia po etapie 200 (to samo przejście co w Expert) rozkodowane programowo (liczniki, bonus, czas, pasek, etykieta). Czasy w klatkach od początku wycierania (T = pwFrame - 225; z frontu wycierania: pwFrame = 8*i + 57 dla i-tego zdjęcia):
  - liczniki klocków wracają po jednym typie: typ 2 przy T=-5, typ 4 przy 3, typ 3 przy 43, typ 1 przy 51, typ 0 (duże trójkąty) przy 107 (zgodne z klatką użytkownika t=100, gdzie pierwszy licznik jeszcze 0);
  - pasek czasu odsłania się od PRAWEGO końca w lewo, 7/15 kolumny na klatkę, od T=-10;
  - czas skacze 0 -> 100 i bonus 0 -> ok. 1/6 -> pełny przy T=59 / 75;
  - etykieta 200 -> EXP zmienia znaki od prawej: T=83, 103, 127 (tylko po etapie 200; w zwykłym Expert etykieta już jest EXP).
- Zegar etapu rusza po zakończeniu wycierania (potwierdzone: pierwszy spadek czasu ok. 100 klatek po końcu wycierania).
- Implementacja: Game::refillPanel; test: 3 klatki z oryginału (tests/data/screen_beyond200_i{24,30,36}.rgb), <= 60 różnych pikseli w liczn./pasku/etykiecie (wynik: 0, 7, 13).
- Zysk za etap w Expert (52 vs 55): to zapewne tylko inny czas ułożenia – wzór stageGain (25*etap + kroki*(etap/2+1)) zgadza się z Novice; bez osobnej poprawki.

## Etap 30 – zanikanie do czerni przy zmianie sceny (na życzenie użytkownika)
- Menu -> etap: po Enter/PPM menu zanika do czerni (25 klatek), etap pojawia się z czerni (25 klatek); zegar etapu rusza dopiero po wyjściu z czerni.
- Etap bonusowy: zanikanie sceny przed bonusem (już było), bonus pojawia się z czerni; po bonusie zanikanie (już było) i następny etap pojawia się z czerni.
- Między zwykłymi etapami zanikania NIE ma (żaluzje / wycieranie bez zmian).
- Po etapie 200: po naliczeniu bonusu zanikanie do czerni, potem prawdziwy tryb Expert (expert_ = true: etykieta EXP, tabela B, kolejne etapy z wycieraniem) z losowym etapem pojawiającym się z czerni. UWAGA: w symulatorze oryginału (zdjęcia co 8 klatek) po etapie 200 NIE było zanikania, tylko wycieranie od lewej z etykietą 200 -> EXP – wprowadzone wg polecenia użytkownika, który zna oryginał; poprzednia wersja (wycieranie + zmiana etykiety znak po znaku) została zastąpiona.
- Test: jasność klatek (menu -> etap, bonus, etap 200 -> Expert: minimum < 5-10% jasności; między etapami > 60%).
- Ikony 13-32 prawdopodobnie pochodzą z crackro (cracktro) dołączonego do tej wersji gry (informacja użytkownika) – nie używane w grze.

## Etap 31 – koniec czasu
Zmierzone w symulatorze (etap 1): po upływie czasu (100 tyknięć) bonus odlicza się w dół o 1 punkt na tyknięcie (75 klatek), pierwszy krok w chwili końca czasu; wynik się nie zmienia. Gdy bonus dojdzie do 0 – krótka pusta (czarna) klatka, potem "PLAY WITH SCOREDISK", dalej tabela wyników i menu.
Port: `toFrame_` w trybie Play; w czasie odliczania brak obsługi myszy/klawiszy. Krok odliczania = `level` punktów (25 kroków dla każdego etapu) – to założenie, zmierzono tylko etap 1.

## Etap 32 – dolne rogi figur
Symulator Go wypełnia wielokąty własnym wypełnianiem (hle_area.go, reguła półotwarta), więc gubi ostatni wiersz rogów skierowanych w dół; port to odziedziczył (dla 33 sprawdzonych etapów był identyczny z symulatorem). Teraz `Game::fillPoly` dorysowuje dolny wiersz każdego lokalnego maksimum (wierzchołek lub płaska krawędź) – dla figury, cienia i klocków. Logika pokrycia (raster.cpp) bez zmian. Brak odniesienia ze sprzętu Amigi – zmiana wynika ze zgłoszenia użytkownika.

Etap 33: Esc w etapie bonusowym działa w każdej fazie (także podczas doliczania punktów), nie tylko podczas gry w pary.

## Etap 34 – tempo czasu zależy od etapu
Pomiar na oryginale (sim Go, write-watch na liczniku E = 0x11009d, wszystkie 200 etapów, krok = klatki 50 Hz): E rośnie co 76 klatek na etapach 1–20, 72 (21–40), 68 (41–60), 64 (61–80), 61 (81–100), 58 (101–120), 54 (121–140), 50 (141–160), 38 (161–180), 31 (181–198), 24 (199, 200). Zgadza się z pomiarem użytkownika w WinUAE (etap 1: ok. 160 s, etap 200: ok. 50 s). Odliczanie bonusu po końcu czasu: co ten sam okres, o `level` punktów (zmierzone na etapach 1, 20, 199: 25 kroków). Zastąpiło to stałe 75 klatek. Etap 63 w pomiarze wyszedł poza serię (68 zamiast 64) – uznany za artefakt; do sprawdzenia.

Etap 35: napis GAME PAUSED – kolor 0 obrazka (litery) rysowany na czarno zamiast przezroczysty.

## Etap 36 – tempo czasu: tabela na każdy etap
Ponowny pomiar etapów 61–64 (powtarzalny): etap 61 = 68, 62 = 64, 63 = 68, 64 = 64 – to nie artefakt, tylko wyjątki w oryginale (figury etapów 59–63 zgodne z oryginałem). `tickFrames` korzysta teraz z tabeli 200 wartości (scoring.cpp), nie z przedziałów po 20. Zysk Expert 52 vs 55: nadal bez osobnego dowodu; wymagałoby ułożenia losowego etapu Expert w symulatorze.

Etap 37: znak '@' w nazwie rysowany jako pełny czarny blok 8x7 (font.cpp, Game::text); ekrany wpisywania imienia są teraz identyczne z oryginałem (100%), test wymaga pełnej zgodności.

## Etap 38 – pełny test 2 graczy
Rozwiązanie etapu 2 odczytane ze zrzutu użytkownika (klasyfikacja kolorów klocków, dopasowanie typ/obrót/pozycja po IoU, potem solver_fast z SOLVE_HINTS, R=3): tests/data/level2_solution.txt. Test 2P: gracz 1 etap 1 → gracz 2 etap 2 (rozwiązany, wynik gracza 2 > 0, wynik gracza 1 zachowany) → gracz 1 etap 3 z własnym wynikiem, wynik gracza 2 zachowany.

## Etap 39 – deasembler 68000 i wyzwalacz etapu bonusowego
- tools/dis68k.cpp (cel `dis68k`): deasembler MC68000 (składnia Motorola). `dis68k <obraz> <start> <koniec> [baza]`, adresy szesnastkowo; obraz = zrzut pamięci gościa (skrypt symulatora `dump plik` zapisuje 2 MB; adres = offset). Kod gry: 0x100008..0x130008 (hunk 0 + dane). a5 = 0x117980 (zmienne globalne: a5-$78fe = numer etapu (bajt), a5-$78e3 = licznik czasu E, a5-$794a = licznik bonusu, a5-$78d6 = numer gracza 1/2, a5-$794e = flaga Expert).
- Licznik czasu: pętla główna (0x1005b2) porównuje `bieżący czas - ostatni tick` z a5-$792a (= wartość z tabeli etapu * 3) i wywołuje 0x102f2e (E++), tabela daje okresy z Etapu 34. Po E>=101 kod 0x103032 odejmuje od bonusu `numer etapu` na tyknięcie (zgodne z pomiarem).
- Etap bonusowy: po ukończeniu etapu (0x100756) rośnie licznik per gracz (tablica a5-$7858, słowo [gracz]); gdy dojdzie do 10 (0x10076c `subi.w #10`) licznik jest zerowany i startuje etap bonusowy (0x1007c2..). Licznik zerowany przy KAŻDYM starcie gry (0x101ae4, także przy starcie z hasła), nie zależy od numeru etapu. Zgodnie z kodem bonus jest co 10-ty UKOŃCZONY etap w grze (np. start od 009 → bonus po 18, start od 019 → po 28). Zgłoszenie użytkownika (start od 009 → bonus po 010) tego NIE potwierdza – do weryfikacji.

## Etap 40 – etap 010 nie dawał się ukończyć

- Ułożenie gracza ze zrzutu (dopasowane co do piksela: `tests/data/level10_user.txt`) jest poprawne, ale wystarczy, że jeden duży element leży 1 piksel obok, a odkryte zostaje 1,09% figury – wcześniejszy próg 1% to odrzucał (widoczny cienki czerwony pasek).
- W oryginale (pomiar na etapie 1, `level1_game_tolerance.txt`) przesunięcie dowolnego elementu o 1 piksel jest zawsze akceptowane. Prawdziwa reguła oryginału nie jest czystym procentem odkrytych pikseli (akceptuje 1,29%, a odrzuca 0,89%) – nadal nie jest odczytana z kodu.
- Próg zmieniony na 1,1% (`solved(c, uncoveredPerMille = 11)`): zgodność z oryginałem na etapie 1 to 80/91 (było 81/91), a każde przesunięcie o 1 piksel na etapie 10 przechodzi. Próba „przyciągania” elementów o ±1 piksel dawała tylko 42/91 – odrzucona.
- Test: etap 10 z równoległobokiem przesuniętym o 1 piksel w lewo musi zostać zaliczony.
- Weryfikacja w symulatorze (oryginalny kod, dodane polecenia skryptu `poke`/`peek` w kopii symulatora): ułożenie gracza etapu 10 jest akceptowane; każde z 8 przesunięć jednego elementu o 1 piksel – akceptowane; 4 przesunięcia o 2 piksele – odrzucone. Port z progiem 1,1% zgadza się we wszystkich 12 przypadkach (`level10_game_tolerance.txt`, test w test_pieces).

## Etap 41 – etap bonusowy sprawdzony w symulatorze
- Licznik ukończonych etapów: wskaźnik w a5-$7858 (0x110128) → tablica 0x12c022, słowo gracza 1 pod 0x12c026.
- Wpisanie 9 do licznika przed ukończeniem etapu 001 → zaraz po nim startuje etap bonusowy (symulator zatrzymuje się na nieobsłużonym graphics.PolyDraw, którego używa etap bonusowy).
- Start z hasła 009 zeruje licznik (wpisane 7 → 0). Wyjście Esc → menu zostawia licznik, ale kolejny start go zeruje.
- Start z hasła 010 z licznikiem ustawionym na 1 (jak po ukończeniu 009) i ukończenie 010 → etap 011, BEZ etapu bonusowego.
- Wniosek dla tego ADF (md5 77f04f86b108ff4a7364f98899707cdf, użytkownik potwierdził tę samą sumę dla swojego pliku): bonus po każdych 10 etapach ukończonych w jednej grze, niezależnie od numeru etapu.
- Port przestawiony: `stagesDone_[gracz]` (zerowany przy każdym starcie gry) rośnie w checkSolved; przy 10 → `bonusDue_`, licznik od zera. Dotyczy też 2 graczy (osobny licznik każdego gracza, jak w kodzie – niesprawdzone w prawdziwej grze). Testy: start od 010 z licznikiem 1 → etap 011 bez bonusu; licznik 9 → bonus.

## Etap 42 – prawdziwa reguła zaliczania etapu (odczytana z kodu)
- Gdy leży 7 elementów, pętla główna (0x1005a0) co klatkę woła 0x102c12 → procedura w asemblerze 0x10d074 (tryb 0) → 0x10d3c6. Wynik ≠ 0 → etap ukończony (a5-$78fa = 1). Ta sama procedura w innych trybach robi efekty przejść ekranu (dlatego wcześniej uznałem ją za „efekty”).
- 0x10d3c6 bada EKRAN: pikseli koloru 14 (bitplany 0 i 4 zgaszone, 1–3 zapalone = (136,34,0), widoczna część figury) w pasie 224 x 200. Słowa 32-bitowe, 7 na wiersz; maska wiersza do bufora 0x10d498 (40 bajtów na wiersz), badany jest wiersz poprzedni (wiersz 199 nigdy).
  - Licznik d7 = 75 zmniejszany o każdy widoczny piksel (`lsr.l / subx.w / beq`). SUBX tylko zeruje Z, więc warunek zadziała wyłącznie, gdy licznik dojdzie do 0 dokładnie przy ostatnim (najbardziej lewym) widocznym pikselu słowa; w innym razie licznik się przekręca i liczba pikseli już nie blokuje zaliczenia (błąd oryginału, odtworzony).
  - Dla każdej pozycji słowa z widocznym pikselem: sumator bitowy 0x10f3d8 (2 bity) liczy P + prawy + lewy + górny + dolny; przepełnienie (≥ 4) → porażka. Cienkie paski (1 px) przechodzą, wszystko grubsze odpada.
  - Lewy sąsiad: dolne słowo poprzedniego słowa w wierszu jest przesuwane w pamięci w prawo (lsr.w) – później czytane jako „górny” sąsiad (odtworzone).
  - Trzymany lewy przycisk myszy (CIA $bfe001 bit 6) → porażka w tej klatce.
- Port: `uncoveredMask()` (figura minus elementy, tak jak rysuje port) + `originalSolveTest()`; `solved(level, pieces)` tylko przy 7 elementach. Stary próg procentowy usunięty.
- Weryfikacja: 150 ekranów symulatora (oryginalny kod; 91 z pliku tolerancji etapu 1 + 59 losowych ułożeń etapów 1 i 10 z 1–4 przesuniętymi elementami) – zgodność 150/150 (`tests/data/solve_test_screens.txt`, test_pieces).
- Zastrzeżenie: symulator wypełnia wielokąty uproszczonym algorytmem (hle_area.go), a nie jak blitter Amigi, więc same piksele z symulatora nie są wzorcem rasteryzacji; port rysuje elementy tak, jak zostało to dopasowane do zrzutów z WinUAE. Pojedyncze piksele na krawędziach mogą się różnić od prawdziwej Amigi.

## Etap 43 – Expert i punktacja według kodu
- Flaga a5-$794e: -1 = Novice, 1 = Expert. Tabela okresów zegara (a5-$785e): wartość etapu * 3 jednostek zegara (~200 Hz); zgodne z dotychczasową tabelą klatek.
- Expert (0x102cb6): każdy etap losowy (RND(200)+1); zmienna a5-$78d7 (zerowana tylko przy starcie programu, NIE przy nowej grze) rośnie o 10 na etap, aż do 100; okres = 250 - X jednostek. Zmierzone w symulatorze (wpisywanie X): X=10..100 → 61, 58, 56, 54, 51, 48, 46, 44, 41, 38 klatek na tyknięcie. Port: `stageTick_` ustawiany w startLevel – Expert według tych pomiarów (X trzymane przez całą sesję programu, jak w oryginale), pierwszy losowy etap po 200 według tabeli; test w test_game (61, potem 58).
- Zysk za etap: 0x1030e8 – dopóki E < 101: E += 2 (przy E = 98: +1), bonus += etap/2 + 1 (etap = bajt numeru etapu; w Expert numer losowy). Bonus startowy 25 * etap (0x102d50). Licznik bonusu przechodzi do wyniku przy przygotowaniu następnego etapu (0x102d2a). Wzór portu zgodny; różnica 52/55 to zapewne tylko czas ułożenia.

## Etap 44 – etap bonusowy według kodu (0x104c52..0x105e78)
- Brakujące w symulatorze graphics.PolyDraw dopisane (kopia symulatora) – etap bonusowy działa w symulatorze.
- Ikony DAT 13–32 (10x10) to maski rozmieszczenia: k-ty etap bonusowy gracza w tej grze (licznik a5-$782e, zerowany przy starcie gry, maks. 20) używa DAT 12+k. Piksel koloru 15 = brak kafelka. Pierwsza maska to szachownica (stąd zrzut „szachownicy”), druga spirala, trzecia poziome pasy itd. – sprawdzone w symulatorze (maska nie jest transponowana).
- Pary: (wolne pola)/2, każda para losowy sprite 1..50 (DAT 32+n), oba kafelki w losowych wolnych polach. Kafelek (wiersz r, kolumna c, od 0): (25+18c, 10+18r); czarna ramka 1 px wokół, czarny cień 18x18 od +4,+4, potem sprite.
- Kliknięcie: kolumna (x-6)/18, wiersz (y+9)/18 (sprawdzone kliknięciami w symulatorze). Wybrać można tylko kafelek na brzegu siatki albo z pustym sąsiadem (lewo/prawo/góra/dół). Ramka zaznaczenia na zewnętrznych pikselach kafelka. Drugi wybór: ten sam kafelek → odznaczenie; ten sam sprite → oba znikają, bonus += etap + 10 (po etapie 10: 20 pkt – stąd wcześniejsza obserwacja „+20”); inny → odznaczenie.
- Zegar: okres a5-$7b62 = 200 jednostek (51 klatek), po 20. bonusie mniej o 20 na każdy kolejny; koniec po zdjęciu wszystkich kafelków albo po 101 tyknięciach. Potem pozostały czas przelicza się jak po etapie (0x1030e8: krok co 2 tyknięcia, etap/2+1 pkt), a licznik bonusu trafia do wyniku przy starcie następnego etapu.
- Esc: w pętli etapu bonusowego oryginał nie sprawdza klawiatury; w porcie Esc zostaje (prośba użytkownika).
- Port przepisany według powyższego; testy w test_game zaktualizowane (szachownica, 25 par, etap+10 za parę, czas jak po etapie, druga maska = spirala, limit 101 tyknięć).
- Tempo przeliczania czasu po bonusie w porcie: 1 krok na klatkę (w oryginale nie mierzone).

## Etap 45 – klawisz testowy B (nie ma go w oryginale)
- W trakcie etapu (1 gracz, bez pauzy): B od razu uruchamia kolejny etap bonusowy (po nim gra wraca do następnego etapu).
- W trakcie etapu bonusowego: B przechodzi do następnego układu (punkty z bieżącego bonusu trafiają do wyniku). Układy DAT 13..32 po kolei, potem zostaje 32 – jak w oryginale po 20. bonusie.
- Służy tylko do obejrzenia wszystkich 20 układów; test w test_game.

## Etap 46 – pętla gry, koniec czasu, dwóch graczy, menu (z kodu)
- Klawisze w etapie (0x10067e): `E` = koniec gry (u użytkownika Esc), spacja = pauza, `M` = muzyka, CSI (F-klawisze) = utwory.
- Pauza (0x10433e): panel DAT 7 w (12,67) wchodzi efektem 10 (0x10d1ea: maska otwierająca się od środka każdej 16-pikselowej kolumny, 8 przejść co ~6 klatek, ~46 klatek – zmierzone w symulatorze); koniec pauzy: spacja ALBO przycisk myszy, wyjście tym samym efektem. W trakcie pauzy nic innego (Esc też nie). Port: spacja (oraz dotychczasowe P), klik kończy pauzę, efekt odtworzony, zegar stoi.
- NOWE – koniec czasu (0x103c7e, gdy licznik bonusu spadnie poniżej 0): jeśli wynik gracza ≥ 2 x 25 x etap, panel DAT 84 z dwoma sprite'ami DAT 54 (35,90) i (176,90), napisy „PLAY AGAIN FOR” (59,84) i „nnnn POINTS” (71,93; liczba dopełniona zerami do 4 cyfr), YES/NO; wejście efektem 10. YES (x 86..110, y 106..113): wynik -= 50 x etap, ten sam etap od nowa, licznik bonusu startuje od 0 (nie 25 x etap). NO (x 118..142): 1 gracz → koniec gry (PLAY WITH SCOREDISK). Za mało punktów → od razu koniec gry. Potwierdzone w symulatorze (wynik wpisany do pamięci: panel „PLAY AGAIN FOR 0050 POINTS”); panel portu zgodny poza cieniem pod panelem.
- Dwóch graczy, koniec czasu bez wykupienia: gracz odpada, drugi gra dalej sam jako gracz 1 z wynikiem + numer etapu, od następnego etapu, bez panelu z hasłem (0x103ec2 / 0x1040ee). W ścieżce „za mało punktów” oryginał pokazuje jeszcze tabelę / wpisanie imienia odpadającego gracza – w porcie pominięte.
- Dwóch graczy, koniec gry: najpierw gracz 1, potem gracz 2 może wpisać imię (każdy, jeśli jego wynik się łapie; 0x10088a). Port: kolejka dwóch wpisów.
- Menu (0x101824): klik w parę NOVICE/EXPERT przełącza tryb (nieważne, który napis), klik w parę 1PLAYER/2PLAYER przełącza liczbę graczy (błąd oryginału: dwa razy sprawdzany y, więc w poziomie od x=231 do prawej krawędzi). Start: prawy przycisk albo Enter (bez zmian).
- Testy: PLAY AGAIN (YES: wynik 120 → 70, etap od nowa, po ułożeniu zysk bez 25 x etap; NO → PLAY WITH SCOREDISK; 49 pkt → od razu pytanie), pauza (efekt, Esc ignorowany, klik kończy, zegar stoi).

## Etap 47 – po upływie czasu etap trwa dalej
- Licznik E jest czytany tylko w 0x102f2e (zegar), 0x1030e8 (zysk) i w etapie bonusowym – pętla gry i kładzenie elementów go nie sprawdzają. Po upływie czasu gra toczy się normalnie (elementy, pauza, E/Esc), a bonus spada o numer etapu co tyknięcie; pytanie (PLAY AGAIN / koniec gry) dopiero, gdy bonus spadnie poniżej 0.
- Ułożenie figury w tym czasie: zysk = to, co zostało z bonusu (kroki czasu już nie dochodzą, E = 101).
- Port: zniesiona blokada sterowania w czasie odliczania (zgłoszenie użytkownika: „gra nie reagowała na Esc, nie dało się układać”); pauza zatrzymuje odliczanie; trzymany element nie jest już upuszczany przy końcu czasu. Test w test_game.

## Etap 48 – nazwa pliku exe
- Windows: cel CMake nadal nazywa się `tangram`, ale plik wynikowy to `Tangram_x64.exe` (build 64-bitowy, CMAKE_SIZEOF_VOID_P = 8) albo `Tangram_x86.exe` (32-bitowy) – OUTPUT_NAME w CMakeLists.txt. README zaktualizowane.
- Stary `tangram.exe` w folderze builda to pozostałość po wcześniejszych kompilacjach (CMake nie kasuje starych plików). Krok POST_BUILD celu `tangram` usuwa teraz tangram.exe/.pdb/.ilk z folderu wynikowego.

## Etap 49 – cienie pod panelami
- Panel DAT 84 (PASSWORD, PLAY WITH SCOREDISK / CREATE NEW / INSERT FORMATTED, PLAY AGAIN) oryginał wkleja minterm $C0 (zwykła kopia, bez maski) – piksele koloru 0 są czarne. To one tworzą cień (pas 3 px z prawej i u dołu) i ciemne plamki faktury. Port rysował ten obraz z kolorem 0 przezroczystym. Teraz nieprzezroczyście: panel PLAY AGAIN zgodny z symulatorem w 100% pikseli, panel scoredisk ze zrzutem z oryginału 100% (wcześniej 99,2%).
- To samo dla sprite'ów DAT 54 na panelu PLAY AGAIN i sprite'ów kafelków etapu bonusowego (też $C0).

## Etap 50 – klawisz testowy B usunięty
- Na prośbę użytkownika (układy etapów bonusowych obejrzane). Gra nie reaguje już na B; test klawisza usunięty.

## Etap 51 – tabela dla odpadającego gracza; pytanie o dyskietkę tylko raz
- Dwóch graczy, koniec czasu i za mało punktów na PLAY AGAIN (0x1040ee): odpadający gracz dostaje tabelę wyników (z wpisaniem imienia, jeśli się łapie), po niej drugi gracz gra dalej sam od następnego etapu (wynik + numer etapu). Port: `pendingContinue_` – po wyjściu z tabeli zamiast menu wraca do gry.
- PLAY WITH SCOREDISK oryginał zadaje tylko przy pierwszym końcu gry od uruchomienia (flaga a5-$78c3 ustawiana raz przy starcie, 0x1004c2; pytanie w 0x105e7a). Potem tabela od razu, z wcześniej wybraną opcją. Sprawdzone w symulatorze (druga gra: Esc → od razu HIGHSCORES). Port: `gameOver()` + `askedDisk_`/`useDisk_`.
- Test w test_game (2 graczy, koniec czasu na etapie 1 → pytanie → NO → tabela → gracz 1 gra etap 2 sam; kolejny Esc → od razu tabela).

## Etap 52 – muzyka według kodu
- Gra nie ma efektów dźwiękowych: w kodzie gry (0x100008..0x110000) nie ma żadnego dostępu do rejestrów dźwięku Pauli; cały dźwięk to dwa odtwarzacze Hippela (music1.img / music2.img; wejście +0 = start od początku, +4 = stop).
- Utwór 1 startuje przy uruchomieniu (0x100dde) i gra wszędzie: intro, menu, etapy, ETAP BONUSOWY (0x104c52 nie dotyka muzyki), tabela wyników. Utwór 2 oryginał tylko ładuje; włącza go wyłącznie klawisz.
- Klawisze (tylko w pętli etapu – nie w menu, nie w etapie bonusowym, nie w pauzie): M (0x1005fc) na zmianę wywołuje stop / start bieżącego utworu (start = od początku); klawisz CSI (0x100612: F1–F10, strzałki, Help) zatrzymuje bieżący utwór i startuje drugi od początku. Wybrany utwór zostaje do następnej zmiany (także w menu i w kolejnych grach).
- Port: M = stop/start od początku (zamiast wyciszania), F2 (zgodnie z wcześniejszym poleceniem użytkownika tylko F2) = drugi utwór od początku, oba tylko w trakcie etapu; utwór 2 nie włącza się już sam w etapie bonusowym. Głośność 25% bez zmian. Test w test_game.

## Etap 53 – długość utworów i muzyka w etapie bonusowym (sprawdzone w symulatorze)
- Kopia symulatora dostała log zapisów do rejestrów dźwięku (PAULALOG). Okres zapętlenia (pierwsze powtórzenie sekwencji zapisów):
  utwór 1: oryginał 274,5 s, port 274,4 s; utwór 2 (włączony klawiszem F2 w symulatorze): oryginał 115,8 s, port 115,8 s. Długości są prawidłowe (utwór 1 = 4:34, utwór 2 = 1:56, potem od miejsca pętli).
- Etap bonusowy w symulatorze (PCSET w czasie bonusu): wykonuje się kod odtwarzacza utworu 1 (0x18a090..), kod utworu 2 (0x25df20..) ani razu. Użytkownik pamięta utwór 2 w bonusie – na tym ADF kod tego nie robi; czekam na decyzję.

## Etap 54 – utwór 2 w etapie bonusowym (decyzja użytkownika)
- Na wyraźne polecenie użytkownika etap bonusowy gra utwór 2 (od początku), po nim wraca wybrany utwór (od początku). W kodzie oryginału tego nie ma (Etap 53) – świadome odstępstwo. M wyłącza muzykę także dla bonusu.

## Etap 55 – rysowanie wielokątów porównane z prawdziwą Amigą (zrzuty z WinUAE)
- Zrzut etapu 003 (pusta figura, powiększenie dokładnie 4x, przesunięcie -1 px w poziomie): czerwona figura i czarny cień portu zgodne z WinUAE co do piksela (6381 px figury, 1555 px cienia); różnice tylko pod kursorem myszy.
- Zrzut etapu 002 (7 elementów, 4x): po korekcie pozycji odczytanych wcześniej z tego zrzutu (dwa małe trójkąty o 1 px wyżej, średni trójkąt o 1 px w lewo i w górę) zgodność elementów do 4 pojedynczych pikseli na ostrych narożnikach (rozmycie JPEG).
- Zrzut etapu 200 (7 elementów, 3x): po podobnej korekcie (±1 px na 6 elementach) – różnice tylko pod kursorem i 5 pikseli rozmycia.
- Wniosek: wypełnianie wielokątów w porcie (fillPolygonDrawn: wiersze półotwarte, prawy koniec odcinka włącznie, dolny wiersz lokalnych minimów ekranowych) daje to samo co AreaEnd prawdziwej Amigi – zmiana nie jest potrzebna. Różnice z wcześniejszych porównań wynikały z niedokładnie dopasowanych pozycji elementów, nie z algorytmu.
- tests/data/level2_solution.txt i level200_solution.txt poprawione na pozycje zgodne ze zrzutami (oba nadal zaliczane).

## Etap 56 – reszta kodu: elementy, panel hasła
- Obsługa myszy w etapie (0x101bd0..0x102410): oryginał działa „klik – klik” (LPM na tacce podnosi, LPM na planszy kładzie, LPM na położonym elemencie podnosi go, klik w tackę z elementem tego typu w ręce odkłada go); port zostaje przy przeciąganiu (wcześniejsza decyzja użytkownika), reguły poniżej przeniesione:
  - element kładzie się tylko, gdy wszystkie jego wierzchołki leżą w x 6..221, y 6..193 (0x10242a); inaczej w oryginale nic się nie dzieje, w porcie element wraca na tackę;
  - kolejność rysowania: nowo położony element jest rysowany na wierzchu; podniesienie elementu przerysowuje planszę w kolejności slotów (duży, duży, kwadrat, średni, mały, mały, równoległobok; 0x10259c → 0x102958). Podnosi się element, którego kolor jest pod kursorem (= wierzchni);
  - PPM obraca trzymany element o 1/16 obrotu raz na wciśnięcie (licznik a5-$78b9 nie pozwala na powtarzanie);
  - trzymany element zastępuje kursor myszy (0x102872) – port nie rysuje już kursora nad trzymanym elementem.
- Panel hasła (0x104190): zamyka go dowolny przycisk myszy albo dowolny klawisz (wcześniej w porcie tylko LPM / Enter).
- Tabela wyników / wpisywanie imienia (0x1034a2.., 0x105e7a..): wyniki porównywane jako napisy dopełnione spacjami (port: liczbowo – dla tych samych długości to samo); ekrany już zgodne ze zrzutami w 100%.
- Test: element wystający poza pole nie zostaje położony.

## Etap 57 – podnoszenie elementu za każdy piksel
- Zgłoszenie: rombu (kwadratu) nie dało się wyciągnąć za dolny róg. Przyczyna: test „punkt w wielokącie” (środek piksela) pomijał piksele krawędzi rysowane przez wypełnianie (prawy koniec odcinka, dolny wiersz narożnika) – dla każdego elementu 40–120 pikseli nie dawało się chwycić.
- Teraz port sprawdza dokładnie narysowane piksele elementu (fillPolygonDrawn), jak oryginał, który patrzy na kolor pod kursorem. Test: kwadrat podnoszony za dolny i prawy róg.

## Etap 58 – równoległobok na tacce chwytany za dolny róg
- Obrazek równoległoboku na tacce sięga y = 148 (dolny róg x 294..302), a prostokąt chwytania kończył się na y = 140. Teraz {294, 88, 315, 149} – jak obszar w oryginale (0x1022aa: x 294..314, y 89..149). Test w test_game.

## Etap 59 – nowe filtry obrazu i klawisze F4–F9
- Klawisze (Win32 i X11): F4 – rozmiar okna (wcześniej F7), F5 – filtr wyłączony, F6 – ostrzejszy (EPX), F7 – wygładzony (bilinearny), F8 – linie CRT, F9 – antialiasing figur. Gra nie dostaje już F5–F9 (i tak ich nie używa; F2 = zmiana utworu bez zmian).
- CRT (scale_filter.cpp): każdy wiersz źródła → n wierszy, ostatni przyciemniony do 50% (przy n ≥ 4 pierwszy do 82%), lekkie poziome rozmycie z sąsiadem, jasność +12%.
- Antialiasing figur (Game::scaleFrame): drawScene zapisuje listę wielokątów (cień, figura, elementy, trzymany element) z kolorami oraz obraz przed nimi i po nich. W oknie: najbliższy sąsiad, a w pikselach przy krawędziach wielokątów (gdzie zmienia się wierzchni wielokąt) – ponowne rysowanie z próbkowaniem 2×2 / 3×3 na piksel okna. Ciągły kształt odpowiada wypełnianiu gry (odcinek do prawej krawędzi + 1, dodatkowy wiersz dołków), więc elementy kryjące figurę na zwykłym obrazie kryją ją też po wygładzeniu (bez czerwonych szczelin). Miejsca zakryte panelem, kursorem lub ściemnieniem zostają bez zmian. Wynik buforowany, gdy scena się nie zmienia; liczony od nowa przy ruchu (Release: ~6 ms przy 1280×800, ~12 ms przy 1920×1200).
- Testy: test_scale (CRT, Antialias bez gry = Off, cykl filtrów), test_game (antialiasing zmienia piksele krawędzi, liczba czerwonych pikseli prawie bez zmian, bufor).
- Poprawka (zgłoszenie użytkownika ze zrzutem): pod ostrymi dolnymi rogami (róg na dole albo obrót o 1 krok) antialiasing dorysowywał kanciasty „ząbek” – kopię kształtu sprzed jednego wiersza. Teraz dodatkowy wiersz dostają tylko płaskie dna (prostokąt dokładnie jak w wypełnianiu), a ostry róg zostaje ostry.
- Poprawka 2 (zrzut z równoległobokiem): wycięty lewy dolny róg i brak kawałka górnego rogu. Gładki kształt liczy teraz krawędzie tak jak wypełnianie gry: to, które krawędzie przecinają wiersz, rozstrzyga numer wiersza (poziome krawędzie zostają na swoim miejscu, cały górny wiersz jest pełny), a punkt przecięcia bierze się pół wiersza wyżej od próbki (gra mierzy na górnej linii wiersza). Płaskie dna dostają dodatkowy wiersz jak w wypełnianiu; ostry róg dostaje dodatkowy piksel tylko wtedy, gdy jeden z jego boków jest pionowy (inaczej zostaje ostry). Sprawdzone na trójkątach i równoległoboku w kilku obrotach; bez czerwonych szczelin pod elementami.

## Etap 60 – antialiasing całego obrazu, F8 cyklicznie, F5 rozmiar okna
- Klawisze (Win32 i X11): F5 – kolejny rozmiar okna (960×600, 1280×800, … do maksimum ekranu, potem znowu 960×600); F8 – kolejny filtr: CRT (domyślny przy starcie) → wygładzony → antialiasing → wyłączony → CRT. F4, F6, F7, F9 nie robią już nic (EPX zostaje w kodzie, ale nie ma go w cyklu).
- Okno startuje w 1280×800 (albo mniejszym, jeśli ekran jest za mały); rozmiaru 640×400 już nie ma (najmniejszy 960×600).
- Antialiasing obejmuje cały obraz (tg::scaleFrame, ScaleFilter::Antialias): reguła xBR poziomu 1 dla dowolnej skali – dla każdego rogu piksela źródła z otoczenia 5×5 rozstrzyga się, czy biegnie tam ukośna krawędź; jeśli tak, róg za linią x+y = 1,5 dostaje kolor sąsiada, z wygładzoną granicą. Płaskie obszary i proste krawędzie zostają bez zmian. Na to Game::scaleFrame nakłada jak dotąd dokładnie wygładzoną figurę, cień i klocki (wynik obu buforowany, gdy obraz się nie zmienia). Odległości ukośne liczone raz na klatkę, wiersze dzielone na wątki (tgcore linkuje Threads). Czas (Release, 2 rdzenie w środowisku testowym): ~6–7 ms przy 1280×800.
- Testy: test_scale (cykl F8, schodek 45° dostaje półtony, płaski obszar bez zmian), test_game bez zmian (czerwone piksele, bufor).
- Przy okazji: test_scale (filtr wygładzony) pisał poza bufor wyjścia (2×1 w skali 4 to 8×4 pikseli, bufor miał 8) – ujawniło się jako awaria po zmianie układu pamięci; bufor poprawiony, test_scale czysty pod AddressSanitizerem.

## Etap 61 – bez antialiasingu i EPX, nowe filtry: 1084S, ostry bilinearny, bikubiczny
- Usunięte na prośbę użytkownika: antialiasing całego obrazu (xBR) i antialiasing figur/klocków (zapisywanie wielokątów w drawScene, Game::scaleFrame z ponownym rysowaniem, bufor) oraz filtr EPX. Game::scaleFrame to teraz tylko wywołanie tg::scaleFrame.
- F8 cyklicznie: CRT (domyślny) → monitor 1084S → ostry bilinearny → bikubiczny → wygładzony → wyłączony → CRT.
- Monitor 1084S (monitor1084): liczony w świetle liniowym (gamma 2,2); poziomo lekkie rozmycie Gaussa (σ = 0,5 piksela źródła, jak ograniczone pasmo monitora); pionowo każdy wiersz źródła to wiązka o profilu Gaussa, szersza dla jaśniejszych kolorów (σ 0,30–0,46 wiersza), więc ciemne wiersze mają przerwy, a jasne prawie się stykają; na końcu maska kratowa (paski R, G, B w kolumnach okna, 78% w „cudzych” kolumnach) i wyrównanie jasności.
- Ostry bilinearny: piksele zostają kwadratowe, mieszane jest tylko po jednym pikselu okna z każdej strony granicy (75/25), więc znika twarde „schodkowanie” bez rozmycia.
- Bikubiczny: Catmull-Rom 4×4, rozdzielnie (najpierw wiersze, potem kolumny), wynik obcięty do 0..255.
- Filtry dzielą wiersze na wątki (parallelRows). Czasy Release przy 1280×800 (2 rdzenie): CRT 2,4 ms, 1084S 7,8 ms, ostry bilinearny 2,5 ms, bikubiczny 3,5 ms, wygładzony 2,7 ms.
- Testy: test_scale (cykl F8, płaski kolor bez zmian w filtrach interpolujących, schodek w ostrym bilinearnym = 0,0,0,64,191,255,255,255, bikubiczny bardziej stromy od bilinearnego, 1084S zachowuje jasność, wiązka i maska), z test_game usunięty test antialiasingu.

## Etap 62 – 1084S domyślny, tylko 4 filtry, okno od 1280×800, pełny ekran
- Filtry (F8): monitor 1084S (domyślny) → CRT → wygładzony → wyłączony → 1084S. Ostry bilinearny i bikubiczny usunięte z kodu.
- tg::scaleFrame przyjmuje dowolny rozmiar wyjścia (outW × outH), nie tylko całkowitą wielokrotność: najbliższy sąsiad, bilinearny i 1084S liczą pozycję piksela źródła ciągle; CRT przyciemnia wiersze w ostatniej 1/n wiersza źródła (n = wiersze okna na wiersz źródła; przy n ≥ 4 także pierwszą 1/n) – w każdy taki pas wpada dokładnie jeden wiersz okna przy każdej skali, więc linie są równe także przy skali 5,4. Dla całkowitych skal wynik jak dotąd (poza środkową kolumną przy nieparzystym n w CRT).
- Okno: najmniejszy rozmiar 1280×800 (960×600 usunięty), F5 przełącza 1280×800 → 1600×1000 → … → maksimum ekranu.
- Pełny ekran: F11 albo Alt+Enter. Win32: okno WS_POPUP na cały monitor (MonitorFromWindow/GetMonitorInfo), powrót przez Get/SetWindowPlacement; X11: _NET_WM_STATE_FULLSCREEN przez menedżer okien. Obraz to największy prostokąt 16:10 mieszczący się w oknie, wyśrodkowany, z czarnymi pasami; filtr liczony od razu w tym rozmiarze (np. 1728×1080 na monitorze 1920×1080), bez późniejszego rozciągania. Mysz przeliczana przez ten prostokąt. Ten sam układ działa po zmaksymalizowaniu lub ręcznej zmianie rozmiaru okna. W pełnym ekranie F5 nic nie robi. Win32: jeśli 1280×800 nie mieści się na ekranie (np. 1366×768), gra startuje w pełnym ekranie.
- Sprawdzone pod Xvfb 1920×1080: okno zmienione na 1920×1080 → obraz 1728×1080 z czarnymi pasami. Testy: test_scale (cykl F8, wszystkie filtry przy 1728×1080 wypełniają cały bufor i nie piszą poza nim, CRT ma dokładnie 200 ciemnych wierszy).

## Etap 63 – README zaktualizowane
- Użytkownik potwierdził: pełny ekran działa na Windows.
- README.md: nowa sekcja „Obraz” (rozmiary okna, pełny ekran, filtry 1084S/CRT/wygładzony/wyłączony), sekcja „Stan” przepisana (była nieaktualna: dwóch graczy, tabela wyników, Novice/Expert są zrobione), `--scale 4`. README_VS2022.txt: `--scale 4`, pełny ekran sprawdzony na Windows.

## Etap 64 – tytuł okna z bitowością
- Tytuł okna: „Tangram 64-bit” w Tangram_x64.exe, „Tangram 32-bit” w Tangram_x86.exe (rozpoznanie po sizeof(void*), w czasie kompilacji); ten sam tytuł w okienku błędu przy starcie. Wersja X11 ma tytuł według tej samej zasady.

## Etap 65 – przegląd kodu: procedury bez opisu (część 1)
- Układ programu: kod gry (skompilowany BASIC) 0x100008..0x10754c (~30 KB, ~9000 linii), biblioteka uruchomieniowa 0x10754c..~0x10d000 (wołana przez jsr d(a4); a4 = 0x10f54c, a5 = 0x117980, ustawiane w 0x10cac0), dalej dane. Biblioteka graficzna wywoływana przez LVO (np. 0x10b8a2 = graphics SetRGB4).
- Mapa: 41 punktów wejścia w kodzie gry; 21 nie miało dotąd opisu (5,5 KB). Przejrzane linia po linii:
  - 0x104664 – figura i cień: x+114 / y = 100−y (cień x+119, y = 105−y), dwa wielokąty kolor 0 i 14; liczona też ramka figury, której nic nie czyta (martwy kod; przy y porównuje współrzędne x – błąd bez skutków). Port zgodny.
  - 0x1048cc / 0x104a74 – ściemnianie / rozjaśnianie palety: 15 kroków, w każdym −1 (albo +1, dopóki składowa celu ≥ krok) na każdej 4-bitowej składowej. **Niezgodność:** port mnożył jasność (num/den). Poprawione: fade() odejmuje teraz tę samą liczbę od każdej składowej 4-bit (ciemne kolory gasną pierwsze). Czas trwania zależy w oryginale od szybkości procesora (SetRGB4 + liczby zmiennoprzecinkowe, bez czekania na ramkę) – zostaje 25 klatek; do zmierzenia w WinUAE.
  - 0x104446 – liczniki elementów na tacce (244,25 / 268,49 / 295,49 / 272,95 / 292,95) i wyniki 7-cyfrowe; zgodne. 0x1045b6 – kopiowanie paneli między buforami (podwójne buforowanie, w porcie zbędne).
  - 0x106a30 – okno pytania o scoredisk: obraz bez figury, panel, pytanie, YES/NO; wejście i wyjście przez rozjaśnienie / ściemnienie; YES: lewy przycisk, y 106..113, x 86..110; NO: x 118..142; potem czeka na puszczenie przycisku. **Niezgodności:** (1) port nie miał ściemniania/rozjaśniania przy pytaniach – dodane (askIn_/askOut_, kliknięcia w czasie przejść ignorowane, akcja po ściemnieniu); (2) port trafiał w obrazki YES/NO zamiast w te prostokąty – poprawione; (3) „CREATE NEW SCORE DISK” i „INSERT FORMATTED DISK” mają x = 30 (w porcie 38 jak „PLAY WITH SCOREDISK”) – poprawione. „INSERT FORMATTED DISK” + NO pyta ponownie (ściemnienie, rozjaśnienie).
  - 0x106c1e/0x106d14/0x106d70 – zapis/odczyt pliku wyników (Open 1006, długość słowem + bajty; 12×3 + 12×2 pól) – zgodny z HighScores::serialize.
  - 0x101b50 – wpisywanie hasła w menu (Backspace, tylko cyfry, max 5) – zgodne.
  - 0x102828 / 0x102aa2 / 0x102ad6 – odczyt kształtu elementu z DATA i obrót o kąt = obrót × 22,5° od kształtu bazowego, zaokrąglanie połówek od zera (0x10b58a) – zgodne z pieceOutline.
  - 0x107012 – podwójne buforowanie przez listę copper (czeka na linię > 220) – w porcie zbędne; 0x1072f8 – kopiowanie prostokąta; 0x1073d2 / 0x1074ac / 0x1074ea / 0x107538 – sprite'y, AllocMem, sprzątanie; 0x102a72 – zapis tła pod kursorem.
- Testy: test_game czeka na rozjaśnienie/ściemnienie pytań (askClick); porównanie panelu i tabeli z prawdziwymi zrzutami nadal 100%.
- Zostało (następne etapy): ponowne czytanie linia po linii dużych procedur, które w notatkach mają tylko pojedyncze odwołania (~24 KB kodu gry), oraz 0x106dc8 (rysowanie tekstu, sprawdzone zrzutami).

## Etap 66 – przegląd kodu (część 2): start, menu, koniec gry, tabela wyników
- 0x10000e (start): ekrany, wczytywanie plików, test pamięci (exec AvailMem → a5-$78bd = 1 przy dodatkowej pamięci: obrazy trzymane w pamięci, pytanie „INSERT FORMATTED DISK”; bez niej gra prosi systemowym requesterem o dyskietki). Port odpowiada wariantowi z dodatkową pamięcią. Pętla główna i koniec etapu (0x10069a..): ściemnienie 0x1048cc, panel hasła tylko w Novice (Expert: bez panelu), co 10 etapów bonus – zgodne z wcześniejszymi etapami.
- Napisy autorów (0x101330..): pozycje DESIGN/MUTSCHALL/MUSIC/HIPPEL/AMIGA/VERSION/BEHRENDS/MAACK = kCredit* w porcie – zgodne.
- Menu (0x1016e4): rozjaśnienie, przełączniki Novice/Expert (y 83..107, x 231..291) i 1/2 graczy (y 116..140, x > 230 – oryginał porównuje y zamiast x), start Return albo prawym przyciskiem, hasło szukane FOR i = 1 TO 995 STEP 5 (kod etapu 200 nie działa), ściemnienie po starcie – wszystko już w porcie.
- Koniec gry dwóch graczy (0x10088a): dla gracza 1 i 2: wpis do tabeli Novice (A) przy Novice, do tabeli Expert (B) przy Expert, tylko gdy VAL(12. wynik) < wynik gracza. **Niezgodność:** port dopuszczał też remis z 12. miejscem – poprawione (endGame i kolejka drugiego gracza). Gdy żaden się nie łapie – sama tabela (zgodne).
- Tabela wyników (0x1034a2..0x103c7c): miejsce = liczba wpisów z wynikiem ściśle większym (porównanie liczb zmiennoprzecinkowych 0x109eb8) – nowy wpis nad równymi, jak w porcie; wiersze y = 42 + 9·i, imię x 76, etap x 150 (tylko Novice), wynik x 188; wpisywanie: Backspace, max 8 znaków, Return kończy, potem zapis pliku, jeśli scoredisk. Samo oglądanie: wyjście klawiszem Return albo dowolnym przyciskiem myszy (MOUSEK), albo po 254 obiegach pętli (zmierzone wcześniej: ~298 klatek). **Niezgodność:** port wychodził też po Esc i tylko lewym przyciskiem – poprawione (Return / dowolny przycisk).
- Obracanie (0x101bd0): prawy przycisk obraca o 1 krok tylko w chwili wciśnięcia (licznik a5-$78b9, bez autopowtarzania) – zgodne.
- Zostało: 0x101d78 (podnoszenie/odkładanie), 0x102410/0x10259c, 0x102958, 0x102c3a (start etapu), 0x102f2e, 0x1030e8, 0x103c7e, 0x104190, 0x10433e, 0x104c52 (bonus), 0x105e7a (wczytanie ekranu tabeli).

## Etap 67 – muzyka z plików WAV (prośba użytkownika)
- Użytkownik dostarczył nagrania: music1.wav (276,5 s, utwór 1) i music2.wav (115,2 s, utwór 2 – bonus); oba 44100 Hz, stereo, 16-bit; głośność (RMS ~3430) taka jak emulowany odtwarzacz przy pełnym wzmocnieniu, więc w grze mnożone ×0,25 jak emulacja.
- Game::loadMusicWav(song, bajty): parser RIFF (fmt/data), 16-bit PCM mono/stereo, inna częstotliwość → liniowo do 44100 Hz. Game::audio: jeśli utwór ma nagranie, gra je od wavPos_ (zerowane przy każdym starcie utworu: start, M, F2, wejście/wyjście z bonusu), po końcu od początku; inaczej emulacja jak dotąd.
- Okna (Win32/X11) wczytują music1.wav / music2.wav z folderu programu albo bieżącego; błędny plik → komunikat i zostaje emulacja. CMake kopiuje assets/music1.wav i assets/music2.wav obok exe (jeśli są). Pliki WAV nie są w paczce zip (69 MB) – użytkownik ma je u siebie.
- Sprawdzone: X11 z --wav: pierwsze 4 s wyjścia = music1.wav / 4 (różnica ≤ 1 z zaokrąglenia). Test w test_game (25%, zapętlenie, mono 22050 Hz, zły plik).

## Etap 68 – przegląd kodu (część 3): tacka, odkładanie, zegar, odliczanie premii
- Tacka (0x101d78): element podnosi się, gdy kursor jest w prostokącie danego elementu (x > a i x < b, y > c i y < d: 234..314 × 5..45, 234..274 × 47..87, 274..314 × 47..87, 234..274 × 89..109, 294..314 × 89..149) **i** piksel pod kursorem ma kolor 0 (kontury, cyfry liczników) albo 20..24 (szare elementy); klik w pergamin obok elementu nic nie robi. **Niezgodność:** port miał inne prostokąty („sylwetki”) i nie sprawdzał koloru – poprawione (trayClick czyta ekran bez kursora). Dolny róg równoległoboku (296,147) i dolny czubek kwadratu (254,87) nadal działają; (254,88) i pergamin – nie, jak w oryginale.
- Odkładanie (0x102410/0x10259c) i przeciąganie: bez zmian względem Etapu 56 (oryginał „klik-klik”, port – przeciąganie, decyzja użytkownika).
- Zegar (0x102f2e): co tick E += 1, F += 0,51 (pasek), czas na panelu; po E = 101 licznik premii maleje o numer etapu na tick, poniżej 0 → 0x103c7e (PLAY AGAIN / koniec) – zgodne.
- Odliczanie po ułożeniu (0x103120..0x10347c): najpierw czas spada krokami po 2 (1 przy E = 98), a licznik premii **rośnie** o etap/2+1 na krok (wynik stoi), pasek czasu się kończy; potem premia przechodzi do wyniku krokami 1000/100/10/1 (największy mieszczący się). **Niezgodność:** port pokazywał to w przybliżeniu (czas i premia malały razem, wynik rósł liniowo) – teraz dokładnie ta sekwencja wartości i pasek z E; czas trwania kroków w oryginale zależy od procesora, więc kroki są rozłożone na zmierzone 156 klatek (koniec po 4/5, potem stoi). Test w test_game (Game::shownNumbers).
- Zostało: 0x102958, 0x10433e, 0x103c7e, 0x104190, 0x104c52 (bonus), 0x105e7a.

## Etap 69 – obrót przy przytrzymanym PPM (decyzja użytkownika)
- Użytkownik pamięta obracanie przy przytrzymaniu PPM. W kodzie tej wersji (0x101bf4) obrót jest przy licznik < 1 lub > 8, a licznik zatrzymuje się na 3 – autopowtarzanie nigdy nie rusza (najpewniej pomyłka autora). Na życzenie użytkownika (wariant „wolniejsze tempo”): pierwszy krok przy wciśnięciu, kolejny po 12 klatkach, potem co 6 klatek; puszczenie przerywa. Świadome odstępstwo. Test w test_game.
- Poprawka kompilacji (MSVC): game.hpp nie dołączał <array> (GCC miał go pośrednio) – dodane; każdy nagłówek sprawdzony osobno.
- Użytkownik potwierdził (WinUAE): długość ściemniania i tempo odliczania premii po ułożeniu są w porządku – zostają (25 klatek, 156 klatek).

## Etap 70 – przegląd kodu (część 4, koniec)
- 0x102958 – rysowanie położonego elementu: kształt bazowy obrócony (0x102ad6), x = środek + x, y = środek − y (oś y kształtu w górę), wielokąt – zgodne z pieceOutline/placedOutline (sprawdzone wcześniej pikselowo).
- 0x104190 – panel hasła: tekst w (26,91), numer etapu w (178,91), hasło w (94,103), efekt panelu, czeka na puszczenie przycisku i pusty bufor klawiatury, potem dowolny przycisk lub klawisz – zgodne (Etap 56).
- 0x10433e – pauza: panel DAT 8 w (12,67), efekt, koniec spacją albo przyciskiem myszy – zgodne (port przyjmuje też P).
- 0x10067e – klawisze w etapie: E = koniec gry, spacja = pauza, M = muzyka, CSI = utwór. **Uzupełnienie:** port przyjmuje teraz także E (oprócz Esc, który zostaje na życzenie użytkownika).
- 0x103c7e – PLAY AGAIN: prostokąty YES/NO (y 106..113; x 86..110 / 118..142), koszt 2 × 25 × etap, dwóch graczy – zgodne z Etapami 46–47 i 51.
- 0x104c52 (bonus) i 0x105e7a (wczytanie ekranu tabeli) – przejrzane w Etapach 44 i 56; ekrany tabeli zgodne ze zrzutami w 100%.
- Przegląd kodu gry (0x100008..0x10754c) zakończony.

### Świadome odstępstwa od oryginału (decyzje użytkownika)
- Przeciąganie elementów (oryginał: „klik – klik”); puszczenie poza polem odsyła element na tackę.
- Esc kończy grę (oryginał: E – działa też); P pauzuje (oryginał: tylko spacja); Esc w etapie bonusowym.
- Utwór 2 w etapie bonusowym (oryginał gra dalej utwór 1); muzyka z plików WAV, jeśli są.
- Obrót przy przytrzymanym PPM (w tej wersji kodu powtarzanie nie działa).
- Hasła z listy użytkownika różniące się jedną cyfrą też są przyjmowane; hasło etapu 200 (95039) działa (w oryginale nie).
- Okno, filtry, pełny ekran, ikona, tytuł – dodatki portu.

## Etap 71 – dźwięk w wersji na Linuksa
- Okno X11 grało muzykę tylko przez --wav / --audio-stdout. Nowe src/audio_linux.*: PulseAudio (także PipeWire) przez libpulse-simple.so.0, inaczej ALSA przez libasound.so.2 – obie ładowane dlopen w czasie działania (własne deklaracje kilku funkcji, bez nagłówków i bez zależności przy kompilacji; CMake linkuje tylko dl i wątki). 44100 Hz, 16-bit stereo; PulseAudio ~60 ms bufora, ALSA 80 ms z odzyskiwaniem po niedoborze; komunikaty ALSA wyciszone.
- Wątek dźwięku renderuje po 512 klatek (Game::audio pod wspólną blokadą z pętlą gry) i czeka w zapisie do urządzenia; dodatkowo nie wyprzedza czasu rzeczywistego o więcej niż 150 ms (urządzenia, które nie blokują). --mute wyłącza dźwięk; przy --frames/--wav/--audio-stdout urządzenie nie jest otwierane.
- Sprawdzone tutaj: brak urządzenia → komunikat i cisza (2 ms), ALSA z urządzeniem null → „sound: ALSA”, gra działa. Prawdziwego odtwarzania nie da się tu usłyszeć – do sprawdzenia u użytkownika.

## Etap 72 – przejście do następnego etapu: 11 losowych efektów (0x1007e8, 0x10d074)
- Zgłoszenie użytkownika: przy pojawianiu się następnego etapu cały ekran miał w oryginale dodatkowe efekty. Kod: po kliknięciu panelu hasła (Novice) albo po odliczaniu (Expert, dwóch graczy) 0x102c3a rysuje nowy etap w drugim buforze, a potem (jeśli to nie powrót z bonusu, a5-$78d3) a5-$78f9 = RND(11)+1 i wywołanie 0x10d074(…, opóźnienie 5000, efekt, wzór $F0F0), które kopiuje nowy obraz na ekran. Panel hasła NIE zwija się żaluzjami – zostaje, a nowy etap przykrywa go efektem. (Panel hasła wchodzi efektem 10 z opóźnieniem 500 – to są „żaluzje”, bez zmian.)
- Efekty (tablica 0x10f416): 1 wiersze 0..199 szachownicą 4 px ($F0F0, odwracaną co wiersz), potem 199..0 resztą; 2 wiersze z góry; 3 z góry i z dołu do środka; 4 z dołu; 5 kolumny od lewej; 6 od prawej; 7 z obu stron do środka; 8 wiersze z góry wzorem $F0F0 i jednocześnie z dołu $0F0F; 9 kolumny od lewej w wierszach nieparzystych i od prawej w parzystych; 10 każda 16-pikselowa kolumna otwiera się od środka (8 przejść); 11 w każdych 32 px po dwie kolumny na przejście – od brzegów do środka (wiersze parzyste) albo od środka na zewnątrz (nieparzyste), 16 przejść.
- Czas: model cykli 68000 (7,09 MHz): pętla opóźnienia 5000 = 0,353 klatki na krok, wiersz 5 płaszczyzn 0,033, kolumna 0,61 (wzór: 0,47), przejście efektu 10/11 przez cały ekran 5,5 (ten model daje 46 klatek pauzy zmierzone wcześniej). Długości: 1 – 155, 2 – 78, 3 – 42, 4 – 78, 5 – 252, 6 – 252, 7 – 224, 8 – 84, 9 – 358, 10 – 53, 11 – 101 klatek. Niepewne (szybkość procesora/pamięci chip) – do porównania z WinUAE.
- Port: Game::transitionShowsNew / transitionFrames; Novice: zamiast zamykania żaluzji (closeFrame_) efekt na całym ekranie (stary obraz = panel hasła); Expert / dwóch graczy: zamiast „wjazdu od lewej” (to był efekt 5 – ten wylosował się w nagraniu z symulatora) losowy efekt od klatki 225. Test: efekty 1..11 (początek / połowa / koniec), klatki z symulatora dla efektu 5 (99,6% / 98,4%; symulator jest wolniejszy, więc porównanie po tej samej kolumnie), testy przejść czekają na koniec efektu.

## Etap 73 – cień pod kursorem myszy
- Zgłoszenie użytkownika: pod kursorem brakuje cienia. Kod: kursor to bob rysowany przez 0x1073d2 → 0x10f48a (graphics BltMaskBitMapRastPort, minterm $E0) przez maskę, która obejmuje także kształt kursora przesunięty o 2 px w lewo i 6 px w dół; w tym miejscu piksele boba mają kolor 0, więc kursor rzuca czarny cień. Port: Game::drawPointer (cień kolorem 0 tam, gdzie przesunięty kształt nie pokrywa samego kursora, potem kursor) – dla kursora gry i kursora menu.
- Sprawdzone na prawdziwym zrzucie (panel scoredisk, mysz w 90,130): obszar 30×30 wokół kursora zgodny co do piksela.


## Etap 74 – wersja 2.0: dane gry w folderze data (prośba użytkownika)
- Projekt zapisany jako Tangram_x86-64_v2.0 (kopia v1 z Etapu 73). Osiem plików z dyskietki (TANGRAM.DAT, TANGRAM.MLU, TITLEPIC.LO5, THALION.LO5, MUSIC1.IMG, MUSIC2.IMG, HIGHSCOREPIC.LO5, SCORE.DTA – bajt w bajt te same, które były wkompilowane) leży w data/, razem z music1.wav / music2.wav (przeniesione z assets/). Usunięte z programu: src/embedded_assets.*, tools/pack_assets.cpp, tests/test_embedded.cpp.
- src/data_files.*: loadDataFolder (czyta osiem plików, przy braku – lista brakujących i podpowiedź), findDataFolder (data/ obok programu, inaczej data/ w bieżącym folderze). Okna Win32/X11: domyślnie folder data, ścieżka .adf w wierszu poleceń zamiast niego; pliki WAV szukane w data/, obok programu, w bieżącym folderze.
- CMake: po kompilacji kopiuje data/ obok exe (copy_directory_if_different od CMake 3.26, inaczej copy_directory). Exe X11 1,1 MB → 0,36 MB.
- Test test_datafolder (gra startuje z folderu data, brakujące pliki zgłaszane, z TANGRAM_ADF – zgodność bajtowa z dyskietką). Sprawdzone: X11 uruchomiony z innego folderu znajduje data obok programu; sam program bez data – komunikat o brakujących plikach.
- Plik level10_user_shift.txt w katalogu projektu był pozostałością testu (test_game zapisywał go w bieżącym folderze). Usunięty; test zapisuje teraz plik tymczasowy w folderze tymczasowym systemu i go kasuje.

## Etap 75 – folder wyjściowy bin / bin64, Release bez testów
- CMakeLists przebudowany: gra (Tangram_x86.exe / Tangram_x64.exe, na Linuksie tangram_x11) trafia do bin/ (32-bit) albo bin64/ (64-bit) w folderze projektu, razem z kopią data/; pliki .pdb do folderu kompilacji, więc w bin zostaje samo exe + data (w Debug MSVC może jeszcze dodać .ilk). Dla generatorów wielokonfiguracyjnych ustawione RUNTIME_OUTPUT_DIRECTORY_<CONFIG>, żeby nie powstawał podfolder Release.
- Opcja TANGRAM_BUILD_TESTS: domyślnie OFF dla Release / RelWithDebInfo / MinSizeRel (budowana jest tylko gra), ON dla Debug. Usunięte zbędne już kasowanie starego tangram.exe. .gitignore: bin/, bin64/.
- Sprawdzone: Release na Linuksie – tylko biblioteki gry i bin64/tangram_x11 + bin64/data; Debug – wszystkie 15 testów przechodzi.

## Etap 76 – instalator (Inno Setup) i zapis wyników w folderze użytkownika
- installer/tangram.iss: Inno Setup 6.3+; bierze bin64\Tangram_x64.exe i/lub bin\Tangram_x86.exe (preprocesor sprawdza, które są; tylko x64 → instalator tylko dla 64-bit), instaluje jako Tangram.exe, wersja 64-bit na 64-bitowym Windows; polski i angielski kreator, Program Files albo tylko dla użytkownika (PrivilegesRequiredOverridesAllowed=dialog), skróty, deinstalator, uruchomienie po instalacji. Ten sam AppId w v1 i v2.0 – nowsza wersja zastępuje starszą. Wynik: installer/output/Tangram_Setup_vX.exe. Nie testowane tutaj (Inno Setup działa tylko na Windows).
- Tabela wyników: Windows %APPDATA%\Tangram\tangram.sco (ścieżka szeroka – _wgetenv + std::filesystem, działa z polskimi znakami w nazwie użytkownika), Linux $XDG_DATA_HOME/tangram albo ~/.local/share/tangram; folder tworzony przy zapisie; stary tangram.sco z bieżącego folderu wczytywany, gdy nowego brak. Powód: w Program Files gra nie może pisać obok exe.
- CMake: przed linkowaniem tworzy bin/ (bin64/), gdyby został usunięty po konfiguracji.

## Etap 77 – hasło etapu 200 działa (decyzja użytkownika)
- Oryginał odrzuca hasło 95039 (etap 200), choć pokazuje je po etapie 199 (0x101b04: FOR i = 1 TO 995 STEP 5). Na prośbę użytkownika poprawione: 95039 uruchamia etap 200. Świadome odstępstwo. Sprawdzone: wszystkie 200 haseł z listy uruchamia właściwy etap.
