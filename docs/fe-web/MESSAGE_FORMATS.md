# fe-web WebSocket Message Formats

Kompletna lista wszystkich komunikatów obsługiwanych przez fe-web w wersji aktualnej.

Format: dla każdego typu komunikatu pokazane są formaty JSON dla kierunku klient→serwer i serwer→klient.

**UWAGA:** Przed wysłaniem jakichkolwiek komunikatów, klient musi przejść autentykację. Zobacz [AUTHENTICATION.md](AUTHENTICATION.md).

---

## 0. Autentykacja (przed handshake)

### Klient → Serwer
WebSocket URL z hasłem w query string:
```
ws://localhost:9001/?password=tajnehaslo123
```

### Serwer → Klient

Sukces (po handshake):
```json
{
  "id": "1706198400-0001",
  "type": "auth_ok",
  "timestamp": 1706198400
}
```

Błąd (zamiast handshake):
```
HTTP/1.1 401 Unauthorized
Content-Type: text/plain

Unauthorized
```

Połączenie zostaje zamknięte.

---

## 1. Synchronizacja serwera

### Klient → Serwer
```json
{
  "type": "sync_server",
  "server": "IRCnet"
}
```
lub dla wszystkich serwerów:
```json
{
  "type": "sync_server",
  "server": "*"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0018",
  "type": "state_dump",
  "server": "IRCnet",
  "timestamp": 1706198400
}
```
Następnie serwer wysyła serię komunikatów: channel_join, topic, nicklist dla każdego kanału.

---

## 2. Wykonanie komendy IRC

### Klient → Serwer
```json
{
  "type": "command",
  "command": "/msg #polska Hello!",
  "server": "IRCnet"
}
```

### Serwer → Klient
Brak bezpośredniej odpowiedzi. Komenda generuje eventy (np. message, channel_mode, whois).

---

## 3. Ping/Pong (keepalive)

### Klient → Serwer
```json
{
  "id": "ping-123",
  "type": "ping"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0001",
  "type": "pong",
  "response_to": "ping-123",
  "timestamp": 1706198400
}
```

---

## 4. Zamknięcie query

### Klient → Serwer
```json
{
  "type": "close_query",
  "server": "IRCnet",
  "nick": "alice"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0025",
  "type": "query_closed",
  "server": "IRCnet",
  "nick": "alice",
  "timestamp": 1706198400
}
```

---

## 5. Wiadomości (publiczne i prywatne)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/msg #polska Witam!",
  "server": "IRCnet"
}
```

### Serwer → Klient

Wiadomość publiczna (kanał):
```json
{
  "id": "1706198400-0005",
  "type": "message",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "alice",
  "text": "Witam wszystkich!",
  "timestamp": 1706198400,
  "level": 4,
  "is_own": false
}
```

Wiadomość prywatna (query):
```json
{
  "id": "1706198400-0006",
  "type": "message",
  "server": "IRCnet",
  "channel": "alice",
  "nick": "alice",
  "text": "Prywatna wiadomość",
  "timestamp": 1706198400,
  "level": 12,
  "is_own": false
}
```

Własna wiadomość publiczna:
```json
{
  "id": "1706198400-0007",
  "type": "message",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "mynick",
  "text": "Moja wiadomość",
  "timestamp": 1706198400,
  "level": 1,
  "is_own": true
}
```

Własna wiadomość prywatna:
```json
{
  "id": "1706198400-0008",
  "type": "message",
  "server": "IRCnet",
  "channel": "alice",
  "nick": "mynick",
  "text": "Moja prywatna wiadomość",
  "timestamp": 1706198400,
  "level": 9,
  "is_own": true
}
```

---

## 6. Join (dołączenie do kanału)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/join #polska",
  "server": "IRCnet"
}
```

### Serwer → Klient

Podstawowy join:
```json
{
  "id": "1706198400-0010",
  "type": "channel_join",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "alice",
  "timestamp": 1706198400,
  "extra": {
    "hostname": "user@host.example.com"
  }
}
```

Join z IRCv3 extended-join:
```json
{
  "id": "1706198400-0011",
  "type": "channel_join",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "bob",
  "timestamp": 1706198400,
  "extra": {
    "hostname": "user@host.example.com",
    "account": "bob_account",
    "realname": "Bob Smith"
  }
}
```

---

## 7. Part (opuszczenie kanału)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/part #polska Żegnajcie",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0012",
  "type": "channel_part",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "alice",
  "text": "Żegnajcie",
  "timestamp": 1706198400,
  "extra": {
    "hostname": "user@host.example.com"
  }
}
```

---

## 8. Kick (wyrzucenie z kanału)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/kick #polska spammer Get out",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0013",
  "type": "channel_kick",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "spammer",
  "text": "Get out",
  "timestamp": 1706198400,
  "extra": {
    "kicker": "operator",
    "hostname": "spam@spam.example.com"
  }
}
```

---

## 9. Quit (rozłączenie użytkownika)

### Klient → Serwer
Nie dotyczy (event generowany przez serwer IRC).

### Serwer → Klient
```json
{
  "id": "1706198400-0014",
  "type": "user_quit",
  "server": "IRCnet",
  "nick": "alice",
  "text": "Connection reset",
  "timestamp": 1706198400,
  "extra": {
    "hostname": "user@host.example.com"
  }
}
```

---

## 10. Topic (temat kanału)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/topic #polska Witamy na kanale!",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0015",
  "type": "topic",
  "server": "IRCnet",
  "channel": "#polska",
  "text": "Witamy na kanale!",
  "timestamp": 1706198400,
  "extra": {
    "topic_by": "operator",
    "topic_time": "1706198350"
  }
}
```

---

## 11. Channel Mode (tryb kanału)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/mode #polska +o alice",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0016",
  "type": "channel_mode",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "operator",
  "timestamp": 1706198400,
  "extra": {
    "mode": "+o",
    "params": ["alice"]
  }
}
```

Przykład z wieloma parametrami:
```json
{
  "id": "1706198400-0017",
  "type": "channel_mode",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "operator",
  "timestamp": 1706198400,
  "extra": {
    "mode": "+ov-b",
    "params": ["alice", "bob", "*!*@spam.com"]
  }
}
```

---

## 12. User Mode (tryb użytkownika)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/mode mynick +i",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0018",
  "type": "user_mode",
  "server": "IRCnet",
  "nick": "mynick",
  "text": "+i",
  "timestamp": 1706198400
}
```

---

## 13. Nick Change (zmiana nicka)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/nick newnick",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0019",
  "type": "nick_change",
  "server": "IRCnet",
  "nick": "oldnick",
  "text": "newnick",
  "timestamp": 1706198400
}
```

---

## 14. Nicklist (lista użytkowników na kanale)

### Klient → Serwer
Nie dotyczy (wysyłane automatycznie po sync_server lub join).

### Serwer → Klient
```json
{
  "id": "1706198400-0020",
  "type": "nicklist",
  "server": "IRCnet",
  "channel": "#polska",
  "text": "@operator +voice alice bob charlie",
  "timestamp": 1706198400
}
```

Format text: prefiksy (@, +, etc.) przed nickami, oddzielone spacjami.

---

## 15. Away (status away)

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/away Jestem AFK",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0021",
  "type": "away",
  "server": "IRCnet",
  "nick": "alice",
  "text": "Jestem AFK",
  "timestamp": 1706198400
}
```

Powrót (brak away):
```json
{
  "id": "1706198400-0022",
  "type": "away",
  "server": "IRCnet",
  "nick": "alice",
  "text": "",
  "timestamp": 1706198400
}
```

---

## 16. WHOIS

### Klient → Serwer
Przez komendę:
```json
{
  "type": "command",
  "command": "/whois alice",
  "server": "IRCnet"
}
```

### Serwer → Klient
```json
{
  "id": "1706198400-0023",
  "type": "whois",
  "server": "IRCnet",
  "nick": "alice",
  "timestamp": 1706198400,
  "extra": {
    "user": "alice",
    "host": "host.example.com",
    "realname": "Alice Smith",
    "channels": "#polska #test @#ops",
    "server_name": "irc.example.com",
    "server_info": "Example IRC Server",
    "idle": "125",
    "signon": "1706198000",
    "account": "alice_account",
    "secure": "1",
    "away": "Jestem AFK",
    "oper": "1"
  }
}
```

Pola w extra są opcjonalne - obecne tylko gdy dostępne.

---

## 17. Query Opened (otwarcie prywatnej rozmowy)

### Klient → Serwer
Nie dotyczy (generowane automatycznie przez irssi).

### Serwer → Klient
```json
{
  "id": "1706198400-0024",
  "type": "query_opened",
  "server": "IRCnet",
  "nick": "alice",
  "timestamp": 1706198400
}
```

---

## 18. Server Status (status połączenia)

### Klient → Serwer
Nie dotyczy (event generowany przez irssi).

### Serwer → Klient

Połączono:
```json
{
  "id": "1706198400-0026",
  "type": "server_status",
  "server": "IRCnet",
  "text": "connected",
  "timestamp": 1706198400
}
```

Rozłączono:
```json
{
  "id": "1706198400-0027",
  "type": "server_status",
  "server": "IRCnet",
  "text": "disconnected",
  "timestamp": 1706198400
}
```

---

## 19. Auth OK (potwierdzenie autentykacji)

### Klient → Serwer
Nie dotyczy (wysyłane automatycznie po handshake).

### Serwer → Klient
```json
{
  "id": "1706198400-0001",
  "type": "auth_ok",
  "timestamp": 1706198400
}
```

---

## 20. Error (błąd)

### Klient → Serwer
Nie dotyczy.

### Serwer → Klient
```json
{
  "id": "1706198400-0028",
  "type": "error",
  "text": "Not connected to any server",
  "timestamp": 1706198400
}
```

---

## Podsumowanie

### Komunikaty Klient → Serwer (4 typy):
1. `sync_server` - synchronizacja z serwerem
2. `command` - wykonanie komendy IRC
3. `ping` - keepalive
4. `close_query` - zamknięcie query

### Komunikaty Serwer → Klient (20 typów):
1. `auth_ok` - autentykacja OK
2. `message` - wiadomość (publiczna/prywatna)
3. `channel_join` - dołączenie do kanału
4. `channel_part` - opuszczenie kanału
5. `channel_kick` - wyrzucenie z kanału
6. `user_quit` - rozłączenie użytkownika
7. `topic` - temat kanału
8. `channel_mode` - tryb kanału
9. `user_mode` - tryb użytkownika
10. `nick_change` - zmiana nicka
11. `nicklist` - lista użytkowników
12. `away` - status away
13. `whois` - informacje o użytkowniku
14. `query_opened` - otwarcie query
15. `query_closed` - zamknięcie query
16. `server_status` - status połączenia
17. `state_dump` - marker zrzutu stanu
18. `pong` - odpowiedź na ping
19. `error` - błąd

### Uwagi:
- Wszystkie komunikaty serwer→klient zawierają `id` i `timestamp`
- Pole `extra` zawiera dodatkowe dane specyficzne dla typu komunikatu
- Komunikaty klient→serwer mogą zawierać opcjonalne pole `id` dla śledzenia odpowiedzi
- Większość akcji klienta odbywa się przez `command` z komendami IRC


