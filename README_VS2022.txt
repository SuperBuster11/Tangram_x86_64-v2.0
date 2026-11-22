Tangram C++ – uruchomienie w Visual Studio 2022 / 2026
================================================
Wymagania: Visual Studio 2022 ze składnikiem "Programowanie aplikacji klasycznych w C++"
(zawiera CMake). Potrzebny jest też Twój plik ADF z grą.

1. Rozpakuj tangram_cpp.zip, np. do C:\tangram_cpp
2. Visual Studio 2022 -> "Otwórz folder lokalny" (Open a local folder) -> wskaż C:\tangram_cpp
   VS sam wykryje CMakeLists.txt i skonfiguruje projekt (okno Output pokaże "CMake generation finished").
3. Zbuduj wszystko: menu Kompilacja -> Kompiluj wszystko (Ctrl+Shift+B).
4. Uruchom narzędzia (pasek u góry: lista "Wybierz element startowy"):
   - extract_pictures.exe  – argumenty: "C:\sciezka\Tangram.adf" "C:\wyniki"  (folder musi istnieć)
   - dump_adf.exe          – argumenty: "C:\sciezka\Tangram.adf" "C:\wyniki"
   Argumenty ustawisz: Debug -> Debug and Launch Settings (launch.vs.json), albo uruchom z wiersza poleceń.
5. Testy: ustaw zmienną TANGRAM_ADF na ścieżkę do pliku ADF, potem Test -> Eksplorator testów
   (albo w "Developer PowerShell": cd out\build\x64-Debug ; $env:TANGRAM_ADF="C:\...\Tangram.adf" ; ctest --output-on-failure).

Nowe w etapie 2: render_levels.exe (argumenty: tests\data\levels.bin  C:\wyniki\figury.png) rysuje wszystkie 200 figur;
test_levels sprawdza tabelę figur (ctest uruchamia oba testy: core i levels).

Uwaga: pełny opis, sterowanie i stan prac: README.md.

--- Okno i gra (etap 17) ---
W Visual Studio 2022 (Otwórz folder z CMake) zbuduj cel "tangram" (Windows, GDI). Uruchom: Tangram_x64.exe (konfiguracja x64) albo Tangram_x86.exe (konfiguracja x86)
(bez argumentów; wersja 2.0: dane gry leżą w folderze data obok exe – CMake kopiuje go przy każdej kompilacji; plik .adf nie jest potrzebny). Kod Win32 nie był kompilowany przez autora – możliwe literówki.
Na Linuksie: cmake + make tangram_x11, ./tangram_x11 [--scale 4].

--- Lista kontrolna po pierwszym zbudowaniu na Windowsie ---
1. Cel "tangram" kompiluje się bez błędów (kod Win32 nie był nigdy kompilowany - możliwe literówki; prześlij mi komunikaty).
2. Okno startuje na środku ekranu, wyświetla logo Thalion, tytuł, kredyty i menu; klik na smoku zaczyna grę.
3. Klocek: wciśnij LPM na tacce, przeciągnij na figurę, puść; PPM obraca; puszczenie poza figurą wraca na tackę.
4. Ukończ etap 1 (hasło 03797 dla etapu 2): odliczanie, żaluzje z hasłem, klik, następny etap; kursor znika na czas hasła.
5. Muzyka gra cicho (25%); M wycisza i przywraca; F2 przełącza utwór cyklicznie; Esc w menu zamyka grę.
6. Menu: wpisz 07274 + Enter -> etap 10; po jego ukończeniu pojawia się (przybliżony) etap dodatkowy.
- Tabela wyników: po Esc w grze odpowiedz YES i wpisz imię; plik tangram.sco powstaje w katalogu uruchomienia.
- Wpisywanie imienia w tabeli wyników: znaki interpunkcyjne idą przez WM_CHAR (kod nieskompilowany, sprawdzić).
- F5: kolejne rozmiary okna 1280x800, 1600x1000, ... do maksimum ekranu (960x600 i 640x400 usunięte).
- F11 / Alt+Enter: pełny ekran (okno bez ramki na cały monitor, obraz 16:10 liczony filtrem od razu w docelowym rozmiarze, czarne pasy); jeśli 1280x800 nie mieści się na ekranie, gra startuje w pełnym ekranie. Sprawdzone przez użytkownika na Windows – działa.
- F8: filtr obrazu cyklicznie – monitor 1084S (domyślny) -> CRT -> wygładzony -> wyłączony.
- Ikona: assets/tangram.ico + assets/tangram.rc (wbudowane w exe). Na Linuksie ikona okna X11 z src/icon_data.hpp (_NET_WM_ICON).

--- Muzyka z plików WAV ---
Włóż music1.wav (utwór 1) i music2.wav (utwór 2, bonus) do folderu data\ projektu – CMake skopiuje je razem z danymi do
folderu data obok Tangram_x64.exe / Tangram_x86.exe. Bez nich gra emulowaną muzykę z oryginału.
Do rozdania gry wystarczy exe + folder data.

--- Gdzie powstaje gra ---
Gotowa gra trafia do folderu projektu: bin\ (x86: Tangram_x86.exe + data\) albo bin64\ (x64: Tangram_x64.exe + data\).
W out\build\... zostają tylko pliki robocze kompilacji. Konfiguracje Release nie budują testów ani narzędzi
(Debug buduje; można to zmienić opcją CMake TANGRAM_BUILD_TESTS=ON/OFF).
