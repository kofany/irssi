# fe-web Testing Guide

Przewodnik testowania modułu fe-web.

## Wymagania

- **irssi** z zainstalowanym modułem fe-web
- **wscat** - narzędzie do testowania WebSocket:
  ```bash
  npm install -g wscat
  ```

## Test 1: Połączenie bez hasła (powinno być odrzucone)

### Krok 1: Uruchom irssi BEZ ustawionego hasła

```
/LOAD fe-web
/SET fe_web_enabled ON
```

### Krok 2: Spróbuj połączyć się bez hasła

```bash
wscat -c "ws://localhost:9001/"
```

### Oczekiwany wynik:

```
error: Unexpected server response: 401
```

W irssi powinno pojawić się:
```
fe-web: REJECTED - No password configured! Use /SET fe_web_password <password>
```

---

## Test 2: Połączenie z błędnym hasłem (powinno być odrzucone)

### Krok 1: Ustaw hasło w irssi

```
/SET fe_web_password tajnehaslo123
```

### Krok 2: Spróbuj połączyć się z błędnym hasłem

```bash
wscat -c "ws://localhost:9001/?password=zlehaslo"
```

### Oczekiwany wynik:

```
error: Unexpected server response: 401
```

W irssi powinno pojawić się:
```
fe-web: Invalid password!
fe-web: [XXXX-XXXX] Authentication failed - closing connection
```

---

## Test 3: Połączenie z prawidłowym hasłem (powinno się udać)

### Krok 1: Połącz się z prawidłowym hasłem

```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

### Oczekiwany wynik:

```
Connected (press CTRL+C to quit)
< {"id":"1706198400-0001","type":"auth_ok","timestamp":1706198400}
```

W irssi powinno pojawić się:
```
fe-web: Password verified successfully
fe-web: [XXXX-XXXX] Handshake completed successfully
fe-web: New connection from 127.0.0.1:XXXXX (id: XXXX-XXXX)
```

### Krok 2: Wyślij komendę sync_server

W wscat wpisz:
```json
{"type":"sync_server","server":"*"}
```

Powinieneś otrzymać:
```json
{"id":"...","type":"server_status","server":"IRCnet","connected":true,...}
```

---

## Test 4: Testowanie polskich znaków (UTF-8)

### Krok 1: Połącz się z prawidłowym hasłem

```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

### Krok 2: Wyślij sync_server

```json
{"type":"sync_server","server":"*"}
```

### Krok 3: Napisz wiadomość z polskimi znakami w irssi

W irssi na kanale #polska:
```
ąćęłńóśźż ĄĆĘŁŃÓŚŹŻ
```

### Oczekiwany wynik w wscat:

```json
{"id":"...","type":"message","server":"IRCnet","channel":"#polska","nick":"TwojNick","text":"ąćęłńóśźż ĄĆĘŁŃÓŚŹŻ","timestamp":...,"level":4,"is_own":true}
```

**Polskie znaki powinny być poprawnie wyświetlone, NIE jako krzaki!**

---

## Test 5: Wysyłanie komendy IRC

### Krok 1: Połącz się i zsynchronizuj

```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

```json
{"type":"sync_server","server":"*"}
```

### Krok 2: Wyślij komendę /whois

```json
{"type":"command","server":"IRCnet","command":"/whois kfn"}
```

### Oczekiwany wynik:

Powinieneś otrzymać serię komunikatów `whois`:
```json
{"id":"...","type":"whois","server":"IRCnet","nick":"kfn","user":"kfn","host":"example.com","realname":"Real Name","channels":["#polska","#test"],...}
```

---

## Test 6: Testowanie wielu klientów

### Krok 1: Otwórz 3 terminale

**Terminal 1:**
```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

**Terminal 2:**
```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

**Terminal 3:**
```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

### Krok 2: Zsynchronizuj wszystkie

W każdym terminalu:
```json
{"type":"sync_server","server":"*"}
```

### Krok 3: Napisz wiadomość w irssi

W irssi:
```
/msg #polska Test wielu klientów
```

### Oczekiwany wynik:

**Wszystkie 3 terminale** powinny otrzymać tę samą wiadomość:
```json
{"id":"...","type":"message","server":"IRCnet","channel":"#polska","nick":"TwojNick","text":"Test wielu klientów",...}
```

---

## Test 7: Zamykanie query

### Krok 1: Otwórz query w irssi

```
/query TestUser
```

### Krok 2: W wscat powinieneś otrzymać

```json
{"id":"...","type":"query_opened","server":"IRCnet","nick":"TestUser","timestamp":...}
```

### Krok 3: Zamknij query przez WebSocket

```json
{"type":"close_query","server":"IRCnet","nick":"TestUser"}
```

### Oczekiwany wynik:

W irssi query powinno się zamknąć, a w wscat:
```json
{"id":"...","type":"query_closed","server":"IRCnet","nick":"TestUser","timestamp":...}
```

---

## Test 8: Keepalive (ping/pong)

### Krok 1: Połącz się

```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

### Krok 2: Wyślij ping

```json
{"type":"ping"}
```

### Oczekiwany wynik:

```json
{"id":"...","type":"pong","timestamp":...}
```

---

## Debugowanie

### Włącz debug w irssi:

```
/SET fe_web_debug ON
```

Zobaczysz szczegółowe logi:
```
fe-web: [XXXX-XXXX] Processing handshake, buffer len: 245
fe-web: Password verified successfully
fe-web: [XXXX-XXXX] WebSocket key: dGhlIHNhbXBsZSBub25jZQ==
fe-web: [XXXX-XXXX] Computed accept key: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
fe-web: [XXXX-XXXX] Handshake response sent (129 bytes)
fe-web: [XXXX-XXXX] Sending message: {"id":"...","type":"auth_ok",...}
```

### Wyłącz debug:

```
/SET fe_web_debug OFF
```

---

## Częste problemy

### Problem: "error: Unexpected server response: 401"

**Przyczyna:** Brak hasła lub błędne hasło.

**Rozwiązanie:**
1. Sprawdź czy hasło jest ustawione: `/SET fe_web_password`
2. Sprawdź czy hasło w URL jest poprawne
3. Sprawdź czy hasło nie zawiera znaków specjalnych wymagających URL encoding

### Problem: Polskie znaki wyświetlają się jako krzaki

**Przyczyna:** Błąd w kodowaniu UTF-8 (powinien być naprawiony w najnowszej wersji).

**Rozwiązanie:**
1. Sprawdź czy masz najnowszą wersję fe-web
2. Sprawdź czy irssi używa UTF-8: `/SET term_charset UTF-8`

### Problem: Nie otrzymuję wiadomości z kanału

**Przyczyna:** Nie zsynchronizowałeś serwera.

**Rozwiązanie:**
Wyślij `sync_server` po połączeniu:
```json
{"type":"sync_server","server":"*"}
```

### Problem: "Connection closed" zaraz po połączeniu

**Przyczyna:** Błąd w handshake lub autentykacji.

**Rozwiązanie:**
1. Włącz debug w irssi: `/SET fe_web_debug ON`
2. Sprawdź logi w irssi
3. Sprawdź czy hasło jest poprawne

---

## Podsumowanie testów

| Test | Co sprawdza | Oczekiwany wynik |
|------|-------------|------------------|
| Test 1 | Brak hasła | 401 Unauthorized |
| Test 2 | Błędne hasło | 401 Unauthorized |
| Test 3 | Prawidłowe hasło | Połączenie OK + auth_ok |
| Test 4 | UTF-8 (polskie znaki) | Poprawne wyświetlanie |
| Test 5 | Wysyłanie komend | Odpowiedź whois |
| Test 6 | Wiele klientów | Wszyscy otrzymują wiadomości |
| Test 7 | Zamykanie query | Query zamknięte |
| Test 8 | Keepalive | Pong w odpowiedzi na ping |

Wszystkie testy powinny przejść pomyślnie! ✅

