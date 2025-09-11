---
type: "always_apply"
---

# irssi - Informacje Krytyczne dla Rozwoju I Implementacji

## Zasady Separacji od Systemowego irssi
Nie używamy nazwy binarnej `irssi` ani standardowych folderów, w których instaluje się oryginalne irssi. Nie używamy również `~/.irssi` jako katalogu domowego. Wszystko po to, by rozwój nie kolidował z systemowym irssi i jego bibliotekami (używamy nowszej wersji niż pakiet zainstalowany).

### Konwencje Nazewnictwa
Plik binarny oraz podstawowa nazwa dla używanych katalogów to **irssi** (od irssi panels):
- Plik binarny: `irssi`
- Katalog instalacji: `/opt/irssi`
- Katalog domowy: `~/.irssi`

### Konfiguracja Środowiska Deweloperskiego
Dla przyspieszenia i ułatwienia testów na żywo, pliki config i default.theme w rzeczywistości znajdują się w naszym workspace:

```bash
ls -la /Users/kfn/.irssi/config
lrwxr-xr-x 1 kfn staff 27 Aug 23 22:13 /Users/kfn/.irssi/config -> /Users/kfn/irssi/config_dev

ls -la /Users/kfn/.irssi/default.theme
lrwxr-xr-x 1 kfn staff 37 Aug 23 02:59 /Users/kfn/.irssi/default.theme -> /Users/kfn/irssi/themes/default.theme
```

## Filozofia Rozwoju

### Zasady KISS (Keep It Simple Stupid)
- Wszelkie wprowadzane zmiany/funkcje mają oddawać ducha prostoty
- Implementacje nie mogą zmieniać działania istniejących mechanizmów
- Zachowanie wstecznej kompatybilności jest priorytetem
- Nowe funkcje powinny być opcjonalne - możliwe do wyłączenia w ustawieniach lub nieaktywne do czasu wywołania

### Proces Deweloperski
1. **Checkpointy**: Przed wprowadzaniem większych zmian zawsze robimy commit roboczy
2. **Testowanie**: Tylko ja testuję wprowadzone zmiany. Ty możesz testować Build lokalnie bez instalacji
3. **Budowanie**: Proces budowania do testów:
```bash
sudo rm -rf /opt/irssi && rm -rf $(pwd)/Build && \
meson setup $(pwd)/Build -Dprefix=/opt/irssi -Dwith-perl=yes -Dwith-proxy=yes && \
ninja -C Build && sudo ninja -C Build install
```

### Pliki Konfiguracyjne
W przypadku zmian wymagających modyfikacji plików config lub theme, edytujemy:
- `/Users/kfn/irssi/config_dev`
- `/Users/kfn/irssi/themes/default.theme`

## Aktualne Zmiany względem Standardowego irssi

### 1. Natywna Obsługa Paneli Bocznych
- **Panel lewy**: Lista okien/kanałów/query z sortowaniem i obsługą myszy
- **Panel prawy**: Lista nicków (nicklist)
- **Cel**: Efekt podobny do WeeChat - łatwe przemieszczanie się po kanałach i query
- **Funkcjonalność**: Klik na element przenosi do okna lub otwiera nowe query

### 2. Modyfikacja Wyświetlania WHOIS
Wyświetlanie outputu komendy whois w aktualnie aktywnym oknie zamiast w oknie status czy sieci

## ✅ Zakończony Projekt: Wyrównanie Nicków w Oknie Czatu

### Status: ZAIMPLEMENTOWANE I DZIAŁA ✅
Stała szerokość pola z nickiem osoby piszącej na kanale z wyrównaniem do prawej została pomyślnie zaimplementowana. Efekt jednej kolumny dla wszystkich wiadomości działa bez konieczności używania zewnętrznych skryptów jak nm2.

## 🚀 Aktualny Projekt: Web Frontend dla irssi

### Cel Implementacji
Stworzenie modułu web frontend dla irssi, który umożliwi dostęp do irssi przez przeglądarkę internetową. Moduł będzie oferował interfejs webowy do zarządzania połączeniami IRC, kanałami i wiadomościami.

### Aktualny Stan (2025-01-25)
- ✅ Podstawowa struktura modułu `fe_web` utworzona
- ✅ Poprawne includes z `irssi/src/...` we wszystkich plikach
- ✅ Funkcje init/deinit/abicheck zaimplementowane
- ✅ Moduł kompiluje się bez błędów
- 🔄 W trakcie: Testowanie ładowania modułu w irssi
- 📋 Następne: Implementacja WebSocket serwera i interfejsu web

### Oczekiwany Efekt Wizualny
```
22:11:35      @yooz │ no działa ;]
22:12:24    @nosfar │ starsze rzeczy ;p
22:12:38      @yooz │ to zamknij oczy
22:14:22        DMZ │ ✅ Link dodany do bazy!
22:14:22      +iBot │ YouTube  Tytuł: LIVE Gemini
22:14:22  LinkShor> │ Skrócony link dla yooz: https://tinyurl.com/yrqbfxeb
```

Alternatywny przykład z nawiasami kątowymi i skróceniem długiego nicka dodatkowy > informuje że to nie pełny nick:
```
21:36:06> <   @kofany> bittersweets outstript
21:36:16> <   @kofany> sarcomas oven's pebble's
21:37:46> <+testNick>> truncation's debarked Allie
21:37:56> <+testNick>> griming surtax's intermediary's
```

### Wymagania Techniczne
- Implementacja WebSocket serwera dla komunikacji z przeglądarką
- Interfejs webowy do zarządzania połączeniami IRC
- Obsługa kanałów, query i wiadomości przez przeglądarkę
- Zachowanie kompatybilności z istniejącymi funkcjami irssi

### Rozwiązanie Finalne Wyrównania Nicków (ZREALIZOWANE)
Implementacja została zakończona sukcesem przy użyciu rozszerzenia systemu wyrównywania irssi z flagą `ALIGN_COMBINE_MODE`. Projekt działa i jest gotowy do użycia.

## Dotychczasowe Próby Implementacji

[Tu następuje szczegółowa dokumentacja techniczna z poprzedniego dokumentu]

## Standardowe Formatowanie Wiadomości irssi
```
# Oryginalny format (bez wyrównania):
msgnick = "%K<%n$0$1-%K>%n %|";
ownmsgnick = "{msgnick $0 $1-}%g";

# Parametry: $0=tryb(mode) (@,+), $1=nick, wynik: <@nick> wiadomość
```

## Zmodyfikowane Pliki

### 1. `/src/core/special-vars.h`
- Dodano flagę `#define ALIGN_COMBINE_MODE 0x10`
- Rozszerzono system wyrównywania o obsługę kombinacji tryb+nick

### 2. `/src/core/special-vars.c`
- Zmodyfikowano `get_alignment_args()` aby parsować flagę '&': `*flags |= ALIGN_COMBINE_MODE`
- Zaimplementowano logikę kombinacji w `parse_special()`:
  - Pobiera tryb z `arglist[2]` i nick z `arglist[0]`
  - Oblicza dostępną przestrzeń: `total_width - mode_len - 2` (dla nawiasów)
  - Wyrównuje do prawej z wypełnieniem: `"   @nick"`
  - Obcina długie nicki: `"@bardzodlu+"`
  - Zwraca połączony string do przetworzenia przez motyw

### 3. `/themes/default.theme`
- Zaktualizowano komendy formatowania używając składni `$[~&12]0`:
  ```
  own_msg = "{ownmsgnick $[~&12]0}$1";
  pubmsg = "{pubmsgnick $[~&12]0}$1";
  ```
- Dodano obszerną dokumentację i komentarze dotyczące dostosowania kolorów

## Próby Implementacji

### Próba 1: Natywny Mechanizm $[]
Początkowo próbowano użyć standardowego wyrównania irssi: `$[-12]0` i `$[-12]1`

**Problem**: Natywne wyrównanie oddzielało tryb od nicka podczas wyrównywania:
```
Wynik: <@     kofany>  # Tryb z lewej, nick wyrównany do prawej osobno
Wymagane: <     @kofany>  # Wszystko razem, wyrównane do prawej
```

### Próba 2: Rozszerzenie ALIGN_COMBINE_MODE
Rozszerzono `parse_special()` aby łączyć tryb+nick przed zastosowaniem wyrównania używając flagi `&`.

**Osiągnięcie**: Pomyślnie utworzono wyrównane do prawej połączone tryb+nick z odpowiednim wypełnieniem i obcięciem.

**Obecna Ściana**: Ograniczenie interpretacji kolorów w systemie motywów.


### Co Mamy Teraz przykład:
```themes
ownmsgnick = "{msgnick %B$0%N%g$1-%N}%g";
```

Z ALIGN_COMBINE_MODE, `$0` staje się połączonym stringiem `"@kofany"`, więc:
- `%B$0%N` koloruje całe `"@kofany"` na niebiesko
- `%g$1-%N` jest ignorowane (puste po połączeniu)
- **Wynik**: `<     @kofany>` gdzie zarówno @ jak i nick są niebieskie

### Czego Potrzebujemy:
```
Oczekiwane: <     @kofany> gdzie @ jest niebieski (%B) a nick jest zielony (%g)
```

### Główny Problem:
Po połączeniu tryb+nick w kodzie C, motyw otrzymuje jeden parametr zawierający `"@kofany"` jako pojedynczy string. Abstrakcje motywu nie mogą zastosować osobnych kolorów do części połączonego stringa - mogą tylko kolorować cały parametr.

## Wyzwanie Techniczne
Fundamentalny konflikt:
1. **Wymóg wyrównania w obecnej implementacji**: Tryb i nick muszą być połączone przed wyrównaniem aby osiągnąć `<     @nick>` a nie `<@     nick>`
2. **Wymóg kolorów**: Tryb i nick potrzebują osobnego formatowania kolorów z abstrakcji motywu
3. **Obecne ograniczenie**: Po połączeniu w C, motyw widzi pojedynczy string i nie może zastosować osobnych kolorów

## HISTORIA PRÓB IMPLEMENTACJI

### PRÓBA 1: Parse Special + ALIGN_COMBINE_MODE (2025-01-24)
**Podejście**: Modyfikacja `parse_special()` z flagą `&` do łączenia mode+nick przed wyrównaniem.
**Problem**: Ograniczenie interpretacji kolorów - po połączeniu mode+nick w jeden string, theme nie może zastosować osobnych kolorów.
**Status**: ❌ Niepowodzenie

### PRÓBA 2: Expandos bez kontekstu (wcześniej)
**Podejście**: Tworzenie expandos `nick_with_mode`, `nick_padding` itp.
**Problem**: Expandos nie mają dostępu do `arglist` z formatowania - zwracają puste stringi.
**Status**: ❌ Niepowodzenie

### PRÓBA 3: Modyfikacja format_get_text_args (2025-01-24)
**Podejście**: Wstrzyknięcie dodatkowego parametru padding do `arglist` w `format_get_text_args()`.
**Problem**: Błędna analiza argumentów - myślałem że `$0=nick`, ale `$0=mode`, `$1=nick`. Przesuwanie argumentów psuło mapowanie.
**Status**: ❌ Niepowodzenie - fundamentalnie błędne założenia

### WNIOSKI Z NIEPOWODZEŃ:
1. **Modyfikacja argumentów formatowania jest ryzykowna** - łatwo zepsuć istniejące mapowanie
2. **Expandos bez kontekstu nie działają** - potrzebują dostępu do aktualnych danych message
3. **Analiza flow musi być dokładna** - błędne założenia prowadzą do niefunkcjonalnych rozwiązań

## AKTUALNE USTALENIA (2025-01-24)

### Rekomendowane Rozwiązanie: Expandos z kontekstem sygnałów
**INSPIRACJA**: Skrypt nm2.pl pokazuje właściwe podejście - expandos + sygnały + dynamiczne przepisywanie formatów.

**KLUCZOWA IDEA**:
Zamiast modyfikować argumenty, stworzyć expando który zwraca gotowy string z paddingiem, mode i nickiem.
