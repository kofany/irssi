# Przegląd: jak Irssi buduje TUI (krok po kroku, funkcja po funkcji)
Poniżej masz rzeczywisty przepływ inicjalizacji, rysowania i obsługi TUI w Irssi (frontend tekstowy), wraz z najważniejszymi funkcjami i ich rolą. Odnoszę się do konkretnych plików i symboli w src/fe-text.

## PUNKT WEJŚCIA
src/fe-text/irssi.c: main()
Rejestracja opcji, setlocale, preinit core
textui_init() → ustawia tryb tekstowy, inicjuje core i sygnały
term_init() → inicjalizacja terminala i prymitywów rysowania
textui_finish_init() → inicjalizacja wszystkich komponentów TUI (okna, statusbar, wejście, rysowanie)
Pętla zdarzeń GLib z dirty_check() po każdej iteracji
int main(int argc, char **argv)
```
{
  // ...
  textui_init();

  if (!term_init()) { /* ... */ }

  textui_finish_init();
  main_loop = g_main_loop_new(NULL, TRUE);

  while (!quitting) {
    // sygnały, HUP/TERM
    dirty_check();
    term_refresh_freeze();
    g_main_context_iteration(NULL, TRUE);
    term_refresh_thaw();
  }

  textui_deinit();
  // ...
}
```

## 1) Inicjalizacja UI (warstwa “frontendu”)
textui_init() w irssi.c
Ustawia irssi_gui = IRSSI_GUI_TEXT, wywołuje core_init(), fe_common_core_init()
Rejestruje sygnały (np. “gui exit”, autoload modułów), ładuje motyw
term_init() w term-terminfo.c

terminfo_core_init(stdin, stdout) → wykrycie możliwości terminala
term_get_size() → rozmiar ekranu
root_window = term_window_create(0,0,w,h) → okno główne terminala
SIGCONT + źródło GLib do redraw; term_common_init() (ustawienia kolorów, SIGWINCH)
Prymitywy rysowania: term_move, term_addstr, term_set_color2, term_refresh, term_window_* itp.
textui_finish_init() w irssi.c

Kolejność komponentów TUI:
textbuffer_init(), textbuffer_view_init()
textbuffer_commands_init(), textbuffer_formats_init()
gui_expandos_init()
gui_printtext_init() (drukowanie tekstu i kolory)
gui_readline_init() (klawiatura, tryb paste, mapy klawiszy)
gui_entry_init() (linia wejściowa)
lastlog_init()
mainwindows_init() (okna główne ekranu)
mainwindow_activity_init()
mainwindows_layout_init() (układ zapis/odtworzenie)
gui_windows_init() (GUI dla logicznych okien Irssi)
statusbar_init() (grupy, konfiguracja, pozycjonowanie i itemy)
dirty_check() → pierwsze pełne odświeżenie
Emituje “irssi init finished” i rysuje statusbar
```
static void textui_finish_init(void)
{
  term_refresh_freeze();
  textbuffer_init();
  textbuffer_view_init();
  textbuffer_commands_init();
  textbuffer_formats_init();
  gui_expandos_init();
  gui_printtext_init();
  gui_readline_init();
  gui_entry_init();
  lastlog_init();
  mainwindows_init();
  mainwindow_activity_init();
  mainwindows_layout_init();
  gui_windows_init();
  // ...
  statusbar_init();
  // ...
  dirty_check();
  // ...
  term_refresh_thaw();
  signal_emit("irssi init finished", 0);
  statusbar_redraw(NULL, TRUE);
  // ...
  term_environment_check();
}
```
## 2) Terminal i prymitywy rysowania (term.*)
Główne API: term.h/term-terminfo.c/term.c
Inicjalizacja: term_init(), term_common_init(), term_deinit()
Rozmiar i resize: term_get_size(), term_resize_dirty(), term_resize(), term_resize_final()
Okna terminala: term_window_create/move/destroy/clear/scroll/…
Rysowanie: term_move, term_addch, term_add_unichar, term_addstr, term_set_color2, term_clrtoeol, term_refresh(window)
Buforowanie odświeżeń: term_refresh_freeze(), term_refresh_thaw()
Klawiatura/tryby: term_set_input_type(), term_set_appkey_mode(), term_set_bracketed_paste_mode()
Sygnały/komendy: SIGWINCH → resize_dirty=TRUE → term_resize_dirty() wywołuje mainwindows_resize()
## 3) Główne okna ekranu i layout (mainwindows.*)
mainwindows_init() w mainwindows.c

Utrzymuje listę GSList *mainwindows, aktywne okno, rozmiar i rezerwacje krawędzi
Bindowanie komend /window ... i sygnałów
Korzysta z term_window_create(...) dla fizycznego obszaru rysowania “mainwindow”
Makra:
mainwindow_create_screen(window) → term_window_create(...) z marginesami statusbara
mainwindow_set_screen_size(window) → term_window_move(...)
Tworzenie i resize

mainwindow_create(int right) → wylicza geometrie, tworzy screen_win
mainwindows_resize(width,height) → dostosowuje wszystkie MAIN_WINDOW_REC
mainwindow_resize_windows() → zleca gui_window_resize() dla logicznych okien, emituje “mainwindow resized”
Reakcje na układ: mainwindows_layout_init()/deinit(), “gui window create override”
Redraw

mainwindows_redraw() i mainwindows_redraw_dirty() w połączeniu z dirty_check()
Po resize: signal_emit("terminal resized"), statusbar nasłuchuje i przelicza się
## 4) Okna logiczne i widok tekstu (gui-windows.*, textbuffer-view.*)
gui_windows_init() w gui-windows.c

Rejestruje sygnały: “window created/destroyed/changed”
Przy tworzeniu okna (gui_window_created()):
Wyznacza MAIN_WINDOW_REC *parent (split/rsplit/nowe mainwindow)
Tworzy GUI_WINDOW_REC z textbuffer_view_create(...)
Jeśli okno jest aktywne w MAIN_WINDOW, podłącza widok do parent->screen_win:
textbuffer_view_set_window(gui->view, parent->screen_win)
Resize okna: gui_window_resize(window, w, h) → textbuffer_view_resize(view, w, h)
Rysowanie treści:

textbuffer_view_redraw(view) wywołuje view_draw_top(...) i term_refresh(view->window)
Wstawianie linii: textbuffer_view_insert_line(...)
Czyszczenie/przewijanie: textbuffer_view_clear/scroll/scroll_line(...)
## 5) Drukowanie tekstu i kolory (gui-printtext.*)
gui_printtext_init() w gui-printtext.c
Rejestruje sygnały “gui print text”, “gui print text finished”, ustawia parametry scrollback
Konwersja formatów na wywołania prymitywów:
Ustalanie atrybutów: gui_printtext_get_colors(&flags, &fg, &bg, &attr)
Fizyczne rysowanie:
term_set_color2(root_window, attr, fg, bg)
term_move(root_window, x, y)
term_addstr(root_window, str)
Czyszczenie linii: term_clrtoeol, term_window_clrtoeol_abs(...)
Obramowania: gui_printtext_window_border(x, y)
## 6) Pasek statusu (statusbar) i jego itemy
statusbar_init() w statusbar.c

Grupy i konfiguracja: statusbar_groups, statusbar_config_init()
Rejestracja itemów: statusbar_items_init() (m.in. “window”, “topic”, “more”, “lag”, “act”, “input”)
Reakcje na sygnały: “terminal resized”, “mainwindow resized/moved”, “gui window created”, “window changed”
Rysowanie:
Obliczenia rozmiarów/priorities: statusbar_resize_items() → statusbar_calc_item_positions()
statusbar_redraw(bar, force) rysuje itemy (przez gui_printtext(...) lub term_*)
Item wejściowy integruje linię wpisu:

statusbar-items.c tworzy GUI entry dla itemu “input”:
rec = gui_entry_create(ITEM_WINDOW_REAL_XPOS(item), item->bar->real_ypos, ...)
gui_entry_set_active(rec); gui_entry_redraw(rec);
Dzięki temu linia wpisu żyje w statusbarze
## 7) Linia wpisu i obsługa klawiatury (gui-entry.*, gui-readline.*)
gui_entry_* (rysowanie linii wpisu)
Rysuje prompt i tekst przez gui_printtext_internal(x, y, str)
Ustawia kursor: term_move_cursor(...) i term_refresh(NULL)
Zmiana pozycji/rozmiaru: gui_entry_move(), gui_entry_set_prompt(), gui_entry_redraw()
// rysowanie promptu i tekstu
gui_printtext_internal(start_xpos, entry->ypos, str->str);
// ...
term_move_cursor(entry->xpos + entry->scrpos + entry->promptlen, entry->ypos);
term_refresh(NULL);
gui_readline_init() (wejście klawiatury)
Konfiguracja trybów: term_set_appkey_mode(...), term_set_bracketed_paste_mode(...)
Mapy klawiszy (strzałki, Home/End, PageUp/Down, Meta-1..0 do zmiany okna, skróty edycyjne)
Obsługa paste: wykrywanie, bracketed paste, limity
Emisja sygnałów (np. “send command”, “gui key pressed”) i operacje na active_entry (gui_entry_*)
## 8) Redraw i pętla zdarzeń
dirty_check() w irssi.c
term_resize_dirty() → ewentualny mainwindows_resize(...)
Przy pełnym redraw: term_clear(); term_refresh(NULL); mainwindows_redraw(); statusbar_redraw(NULL, TRUE);
Następnie: mainwindows_redraw_dirty(); statusbar_redraw_dirty(); term_refresh(NULL);
Pętla GLib:
Przed/po iteracji: term_refresh_freeze()/thaw() dla spójnych aktualizacji
Obsługa sygnałów zakończenia oraz /reload
## 9) Zamknięcie (odwrotność inicjalizacji)
textui_deinit() w irssi.c
Stopniowe *_deinit(): statusbar, entry, printtext, readline, windows/layout, textbuffer_view, terminal (term_deinit()), motyw i core
Ostatni dirty_check() aby wyświetlić ostatnie komunikaty
Kluczowe elementy TUI i ich generatorzy
Główna przestrzeń rysowania: TERM_WINDOW z term_window_create() w term-terminfo.c i wtórne w mainwindows.c (poprzez mainwindow_create_screen)
Okna logiczne: gui-windows.c + textbuffer-view.* (zawartość/chat)
Pasek statusu: statusbar.c + statusbar-items.c (pozycjonowanie i rysowanie)
Linia wpisu: gui-entry.c wkomponowana do statusbara (item “input”)
Rysowanie tekstu: gui-printtext.c (kolory, przetwarzanie, border) + prymitywy term_*
Wejście użytkownika: gui-readline.c (mapy klawiszy, paste, emisja komend)
Sygnały i sprzęgła między modułami
Rozmiar terminala: SIGWINCH → term_resize_dirty() → mainwindows_resize() → sygnały do statusbara
Tworzenie okna: “window created” → gui_window_created() → textbuffer_view_set_window(parent->screen_win)
Rysowanie: “gui print text” → gui_printtext_* → term_*
Statusbar reaguje na: “terminal resized”, “mainwindow resized/moved”, “window changed”
Klawiatura: “gui key pressed”, “send command”, itp.
Dodatkowo: konfiguracja motywów i formatów (themes.h, textbuffer-formats.c), logika aktywności okien (mainwindow-activity.c), ostatni log (lastlog.c).

## Najważniejsze pliki do przeglądu:

src/fe-text/irssi.c, term-terminfo.c, term.c
mainwindows.c, mainwindows-layout.c, gui-windows.c
textbuffer-view.c, gui-printtext.c
statusbar.c, statusbar-items.c, statusbar-config.c
gui-entry.c, gui-readline.c
Jeśli chcesz, mogę narysować mini-callgraph dla konkretnej akcji (np. wpisanie znaku → aktualizacja linii wejścia → repaint). Daj znać której ścieżki działania.

Zmiany lub test: mogę uruchomić build/testy lub dopisać komentarze w kodzie obok funkcji, aby było łatwiej nawigować.

## W skrócie:

### Inicjalizacja: textui_init → term_init → textui_finish_init (textbuffer, printtext, readline, entry, mainwindows, layout, gui-windows, statusbar)
Rysowanie: dirty_check + mainwindows_redraw(_dirty) + statusbar_redraw(_dirty) + term_refresh
Okna: mainwindow_* + gui_window_* + textbuffer_view_*
Statusbar: statusbar_* + itemy (w tym wejście)
Wejście: gui_readline_* + gui_entry_* + term_* tryby paste/appkeys
Najistotniejsze funkcje “generatorzy TUI”:

### Terminal: term_init, term_window_create, term_move, term_addstr, term_refresh
Okna: mainwindow_create_screen, mainwindow_set_screen_size, gui_window_created, textbuffer_view_set_window
Statusbar: statusbar_init, statusbar_redraw, statusbar_calc_item_positions, statusbar_items_init
Tekst: gui_printtext, gui_printtext_get_colors, textbuffer_view_redraw
Wejście: gui_entry_draw, gui_entry_set_prompt, gui_readline_init, term_set_bracketed_paste_mode
Zamykanie: odwrotna kolejność w textui_deinit() i term_deinit().

Krótko: TUI jest zbudowane warstwowo. Terminal dostarcza prymitywy (term_*), “mainwindow” wyznacza obszary rysowania (TERM_WINDOW), gui-windows podpina widoki tekstu (textbuffer_view) do tych obszarów, gui-printtext renderuje formatowany tekst, statusbar zarządza paskami i itemami (w tym linią wejściową poprzez gui-entry), a gui-readline obsługuje klawiaturę i interakcję. Wszystko spina pętla GLib i system sygnałów.

### Najważniejsze punkty:

main → textui_init → term_init → textui_finish_init (kolejno: buffers, print, readline+entry, windows, layout, statusbar)
dirty_check zarządza pełnym i częściowym odświeżeniem
statusbar współgra z mainwindow i gui-entry (linia poleceń jako item paska)
gui-windows + textbuffer-view renderują zawartość okien
gui-printtext + term_* to faktyczne rysowanie
