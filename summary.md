# irssip - Informacje Krytyczne dla Rozwoju I Implementacji

## Zasady Separacji od Systemowego irssi
Nie używamy nazwy binarnej `irssi` ani standardowych folderów, w których instaluje się oryginalne irssi. Nie używamy również `~/.irssi` jako katalogu domowego. Wszystko po to, by rozwój nie kolidował z systemowym irssi i jego bibliotekami (używamy nowszej wersji niż pakiet zainstalowany).

### Konwencje Nazewnictwa
Plik binarny oraz podstawowa nazwa dla używanych katalogów to **irssip** (od irssi panels):
- Plik binarny: `irssip`
- Katalog instalacji: `/opt/irssip`
- Katalog domowy: `~/.irssip`

### Konfiguracja Środowiska Deweloperskiego
Dla przyspieszenia i ułatwienia testów na żywo, pliki config i default.theme w rzeczywistości znajdują się w naszym workspace:

```bash
ls -la /Users/kfn/.irssip/config
lrwxr-xr-x 1 kfn staff 27 Aug 23 22:13 /Users/kfn/.irssip/config -> /Users/kfn/irssi/config_dev

ls -la /Users/kfn/.irssip/default.theme
lrwxr-xr-x 1 kfn staff 37 Aug 23 02:59 /Users/kfn/.irssip/default.theme -> /Users/kfn/irssi/themes/default.theme
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
sudo rm -rf /opt/irssip && rm -rf $(pwd)/Build && \
meson setup $(pwd)/Build -Dprefix=/opt/irssip -Dwith-perl=yes -Dwith-proxy=yes && \
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

## 3. Wyrównanie Nicków w Oknie Czatu z zachowaniem kolorów z motywem (czyli respektowanie theme jak zwykle)

### Status: ZAIMPLEMENTOWANE I DZIAŁA ✅
Stała szerokość pola z nickiem osoby piszącej na kanale z wyrównaniem do prawej została pomyślnie zaimplementowana. Efekt jednej kolumny dla wszystkich wiadomości działa bez konieczności używania zewnętrznych skryptów jak nm2.

## Szczegóły Zakończonego Projektu: Wyrównanie Nicków

### Oczekiwany Efekt Wizualny (ZREALIZOWANY)
```
22:11:35      @yooz │ no działa ;]
22:12:24    @nosfar │ starsze rzeczy ;p
22:12:38      @yooz │ to zamknij oczy
22:14:22        DMZ │ ✅ Link dodany do bazy!
22:14:22      +iBot │ YouTube  Tytuł: LIVE Gemini
22:14:22  LinkShor> │ Skrócony link dla yooz: https://tinyurl.com/yrqbfxeb
```

### Standardowe Formatowanie Wiadomości irssi
```
# Oryginalny format (bez wyrównania):
msgnick = "%K<%n$0$1-%K>%n %|";
ownmsgnick = "{msgnick $0 $1-}%g";

# Parametry: $0=tryb(mode) (@,+), $1=nick, wynik: <@nick> wiadomość
```

### Zmodyfikowane Pliki (ZAKOŃCZONE)
- `src/fe-common/core/fe-expandos.c` - dodano expandos `$nickalign` i `$nicktrunc`
- `src/fe-common/core/formats.c` - automatyczne formatowanie "on-the-fly"
- `src/fe-common/core/fe-messages.c` - kontekst nick/mode dla expandos
- `themes/default.theme` - **NIEZMIENIONY** (działa automatycznie!)

### Rozwiązanie Finalne (ZREALIZOWANE)
Implementacja została zakończona sukcesem przy użyciu **expandos z kontekstem sygnałów**. System automatycznie formatuje wiadomości bez modyfikacji theme. Projekt działa i jest gotowy do użycia.



## 🚀 Aktualny Projekt: Web Frontend dla irssi

### Cel Implementacji
Stworzenie modułu web frontend dla irssi, który umożliwi dostęp do irssi przez przeglądarkę internetową. Moduł będzie oferował interfejs webowy do zarządzania połączeniami IRC, kanałami i wiadomościami.

### Aktualny Stan (2025-01-25)
- ✅ Podstawowa struktura modułu `fe_web` utworzona
- ✅ Poprawne includes z `irssip/src/...` we wszystkich plikach
- ✅ Funkcje init/deinit/abicheck zaimplementowane
- ✅ Moduł kompiluje się bez błędów
- 🔄 W trakcie: Testowanie ładowania modułu w irssi
- 📋 Następne: Implementacja WebSocket serwera i interfejsu web

### Wymagania Techniczne
- Implementacja WebSocket serwera dla komunikacji z przeglądarką
- Interfejs webowy do zarządzania połączeniami IRC
- Obsługa kanałów, query i wiadomości przez przeglądarkę
- Zachowanie kompatybilności z istniejącymi funkcjami irssi