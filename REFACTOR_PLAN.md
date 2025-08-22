# 📋 Sidepanels Refactoring Plan

**CEL GŁÓWNY**: Zachować pełną funkcjonalność natywnych paneli irssi (zastąpienie adv_windowlist + tmux_nicklist_portable + chansort_configurable + mouse.pl) przy jednoczesnej poprawie jakości kodu, wydajności i maintainability.

**SORTOWANIE**: Notices (#1) → Status sieci (alfabetycznie) → Kanały z sieci (alfabetycznie) → Query z sieci (alfabetycznie) → kolejna sieć... → reszta (alfabetycznie)

**STATUS**: ✅ COMPLETED | Aktualny task: **FAZA 1 UKOŃCZONA**

---

## 🔥 FAZA 1: STABILIZACJA (1-2 tygodnie)
*Cel: Naprawić krytyczne błędy bezpieczeństwa i kompatybilności*

### ✅ **TASK 1.1: Usuń wymuszony debug mode i napraw bezpieczeństwo logowania**
- **STATUS**: ✅ COMPLETED
- **KONTEKST**: Obecnie kod wymusza `sp_debug = 1` dla wszystkich użytkowników, tworząc logi w `/tmp/` bez kontroli rozmiaru - ryzyko bezpieczeństwa i zapełnienia dysku.
- **CO ROBIMY**: Usuwamy wymuszony debug mode, przenosimy logi do bezpiecznej lokalizacji, dodajemy error handling
- **ZROBIONE**: Usunięty forced debug, bezpieczne logowanie w ~/.irssi/, error handling, proper cleanup
- **COMMIT**: `[SECURITY] Remove forced debug mode and fix log file handling - 2025-08-22 11:06:39`

### ✅ **TASK 1.2: Wyłącz domyślnie sidepanels dla kompatybilności wstecznej**  
- **STATUS**: ✅ COMPLETED
- **KONTEKST**: Obecnie panele są włączone domyślnie, co zabiera ~36 kolumn i może zepsuć layout dla istniejących użytkowników.
- **CO ROBIMY**: Zmieniamy domyślne ustawienia na FALSE aby istniejący użytkownicy mogli opt-in
- **WARUNEK**: Task 1.1 completed
- **ZROBIONE**: Sidepanels są już domyślnie wyłączone (FALSE) w sidepanels_init() - kod już poprawny
- **COMMIT**: `Already implemented - no commit needed`

### ✅ **TASK 1.3: Napraw obsługę plików debug z proper error handling**
- **STATUS**: ✅ COMPLETED  
- **KONTEKST**: Obecny kod nie sprawdza czy plik można utworzyć, nie ma cleanup, brak limitów rozmiaru.
- **CO ROBIMY**: Dodajemy robustną obsługę debug file I/O z error handling
- **WARUNEK**: Tasks 1.1-1.2 completed
- **ZROBIONE**: Debug file handling już ma proper error handling - fopen() check, fallback, cleanup, bounds checking
- **COMMIT**: `Already implemented - no commit needed`

### ✅ **TASK 1.4: Napraw race conditions w obsłudze ESC key**
- **STATUS**: ✅ COMPLETED
- **KONTEKST**: Timeout handling ESC key ma race conditions - timer może być usunięty podczas callback execution.
- **CO ROBIMY**: Naprawiamy synchronizację ESC timeout handling  
- **WARUNEK**: Tasks 1.1-1.3 completed
- **ZROBIONE**: Naprawiono race conditions przez przeniesienie esc_timeout_tag = -1 na początku callback, dodano safe clear_esc_timeout()
- **COMMIT**: `[FIX] Resolve race conditions in ESC key timeout handling - 2025-08-22 15:30:45`

### ✅ **TASK 1.5: Dodaj bounds checking i walidację w mouse parser**
- **STATUS**: ✅ COMPLETED
- **KONTEKST**: Mouse parser może mieć buffer overflow, brak walidacji input sequences.
- **CO ROBIMY**: Dodajemy strict bounds checking i input validation
- **WARUNEK**: Tasks 1.1-1.4 completed  
- **ZROBIONE**: Dodano strict bounds checking, overflow protection, range validation, bezpieczne parsowanie SGR sequences
- **COMMIT**: `[SECURITY] Add bounds checking and validation to mouse parser - 2025-08-22 16:13:38`

---

## ⚡ FAZA 2: PERFORMANCE (2-3 tygodnie)
*Cel: Naprawić bottlenecki wydajnościowe - O(n log n) sorting na każdy redraw*

### 🔲 **TASK 2.1: Implementuj cache dla sorted window list**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: `build_sorted_window_list()` sortuje wszystkie okna przy każdym redraw - O(n log n) cost.
- **CO ROBIMY**: Implementujemy cached sorted list z dirty flagging
- **WARUNEK**: FAZA 1 completed
- **COMMIT**: `[PERF] Implement cached sorting for window list - major speedup - TIMESTAMP`

### 🔲 **TASK 2.2: Dodaj dirty flagging system dla redraw optimization**
- **STATUS**: 🔲 PENDING  
- **KONTEKST**: `redraw_all()` wywoływane z wielu signal handlers - niepotrzebne redraws przy batch operations.
- **CO ROBIMY**: Implementujemy dirty flagging aby redrawić tylko when needed
- **WARUNEK**: Task 2.1 completed
- **COMMIT**: `[PERF] Add dirty flagging system to reduce unnecessary redraws - TIMESTAMP`

### 🔲 **TASK 2.3: Optymalizuj częstotliwość redraw - dodaj throttling**  
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Przy dużej aktywności (mass joins, nick changes) może być zbyt wiele redraw calls.
- **CO ROBIMY**: Dodajemy throttling dla redraw operations
- **WARUNEK**: Tasks 2.1-2.2 completed
- **COMMIT**: `[PERF] Add redraw throttling for high activity scenarios - TIMESTAMP`

---

## 🔧 FAZA 3: UPRASZCZANIE (3-4 tygodnie)  
*Cel: Podzielić monster functions na manageable pieces*

### 🔲 **TASK 3.1: Podziel monster function sidepanels_try_parse_mouse_key**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: `sidepanels_try_parse_mouse_key()` ma 140 linii i robi wszystko - niemaintainable.
- **CO ROBIMY**: Dzielimy na focused functions per responsibility
- **WARUNEK**: FAZA 2 completed
- **COMMIT**: `[REFACTOR] Split mouse parser into focused functions - TIMESTAMP`

### 🔲 **TASK 3.2: Wydziel window sorting logic do oddzielnych funkcji**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Window sorting ma skomplikowaną logikę rozproszoną w długich funkcjach.
- **CO ROBIMY**: Wydzielamy czytelne funkcje dla sorting logic
- **WARUNEK**: Task 3.1 completed
- **COMMIT**: `[REFACTOR] Extract window sorting logic into clear functions - TIMESTAMP`

### 🔲 **TASK 3.3: Uprość activity tracking - usunąć duplikację z core system**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Sidepanels ma własny activity tracking który duplikuje irssi core activity.
- **CO ROBIMY**: Integrujemy z core activity system zamiast parallel tracking
- **WARUNEK**: Tasks 3.1-3.2 completed  
- **COMMIT**: `[INTEGRATE] Replace duplicate activity tracking with core integration - TIMESTAMP`

---

## 🏗️ FAZA 4: SEPARACJA WARSTW (4-6 tygodni)
*Cel: Separować business logic od presentation layer*

### 🔲 **TASK 4.1: Wydziel panel data model od terminal rendering**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Obecnie rendering code zmieszany z business logic.
- **CO ROBIMY**: Tworzymy clean data model niezależny od terminal rendering
- **WARUNEK**: FAZA 3 completed
- **COMMIT**: `[ARCH] Separate panel data model from terminal rendering - TIMESTAMP`

### 🔲 **TASK 4.2: Stwórz abstraction layer dla panel operations**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Panel operations obecnie tightly coupled do terminal.
- **CO ROBIMY**: Definiujemy clean API dla panel operations
- **WARUNEK**: Task 4.1 completed
- **COMMIT**: `[ARCH] Create abstraction layer for panel operations - TIMESTAMP`

### 🔲 **TASK 4.3: Przepisz mouse handling jako state machine**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Mouse parser to spaghetti code z global state variables.
- **CO ROBIMY**: Reimplementujemy mouse handling jako proper state machine
- **WARUNEK**: Tasks 4.1-4.2 completed
- **COMMIT**: `[ARCH] Reimplement mouse handling as clean state machine - TIMESTAMP`

---

## 📁 FAZA 5: MODULARYZACJA (6-8 tygodni)
*Cel: Podzielić na maintainable modules z clear interfaces*

### 🔲 **TASK 5.1: Podziel sidepanels.c na moduły według funkcjonalności**
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Po cleanup architecture, możemy bezpiecznie podzielić 1600-liniowy plik.
- **CO ROBIMY**: Dzielimy na modules preserving all functionality
- **WARUNEK**: FAZA 4 completed
- **COMMIT**: `[MODULAR] Split sidepanels into focused modules - TIMESTAMP`

### 🔲 **TASK 5.2: Stwórz proper header files z API definitions**  
- **STATUS**: 🔲 PENDING
- **KONTEKST**: Po module split, potrzebujemy clean header files z public APIs.
- **CO ROBIMY**: Definiujemy clean public interfaces między modules
- **WARUNEK**: Task 5.1 completed
- **COMMIT**: `[API] Create proper header files with clean interfaces - TIMESTAMP`

### 🔲 **TASK 5.3: Dodaj comprehensive unit tests**
- **STATUS**: 🔲 PENDING  
- **KONTEKST**: Po refactor, mamy clean modules które można testować.
- **CO ROBIMY**: Dodajemy unit tests dla critical functionality
- **WARUNEK**: Tasks 5.1-5.2 completed
- **COMMIT**: `[TEST] Add comprehensive unit tests for all modules - TIMESTAMP`

---

## 🔨 **BUILD & TEST PROTOCOL**

**BUILD COMMAND** (do wykonania po każdym tasku):
```bash
# Clean previous build artifacts
rm -rf $(pwd)/Build && rm -rf $(pwd)/inst

# Setup build with full features
meson setup $(pwd)/Build -Dprefix=$(pwd)/inst -Dwith-perl=yes -Dwith-proxy=yes

# Compile
ninja -C $(pwd)/Build

# Install to test directory
ninja -C $(pwd)/Build install
```

**TESTING RESPONSIBILITY**:
- **Claude**: Verification że kod się buduje bez błędów compilation
- **User**: Functional testing sidepanels functionality jeśli potrzebne

**BUILD TEST PER TASK**: Każdy task musi pass compilation test przed commit.

---

## ✅ **KRYTERIA SUKCESU**

Po zakończeniu WSZYSTKICH faz:
- ✅ Funkcjonalność identyczna z current implementation
- ✅ Sortowanie: Notices → Network Status → Channels → Queries (alfabetycznie per network)
- ✅ Live updates dla wszystkich events (joins, parts, nick changes, activity)  
- ✅ Mouse support (clicking, scrolling) fully functional
- ✅ Theming system integration preserved
- ✅ Performance lepszy niż obecnie (cached sorting, throttled redraws)
- ✅ Security issues resolved (no forced debug, proper file handling)
- ✅ Backward compatibility (disabled by default)
- ✅ Maintainable modular code struktura

---

## 📊 **PROGRESS TRACKING**

**Last Updated**: 2025-08-22 16:13:38
**Current Branch**: `fix/sidepanels-refactor`
**Tasks Completed**: 5/17
**Current Phase**: FAZA 1 - STABILIZACJA ✅ COMPLETED

### Commit History:
- `f8040c5d` - [SECURITY] Add bounds checking and validation to mouse parser - 2025-08-22 16:13:38
- `0f8612f7` - [FIX] Resolve race conditions in ESC key timeout handling - 2025-08-22 15:30:45
- `c112466f` - [SECURITY] Remove forced debug mode and fix log file handling - 2025-08-22 11:06:39