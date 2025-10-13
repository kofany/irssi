# fe-web State Dump Example

**Scenariusz:** Klient wysyła `sync_server: "*"` i ma 2 sieci połączone z kilkoma kanałami.

**Sieci:**
- IRCal (irc.al) - 2 kanały
- IRCnet (irc.pl) - 3 kanały

---

## Sekwencja Wiadomości Po Połączeniu

### 1. Authentication Success
```json
{
  "id": "1760270400-0001",
  "type": "auth_ok",
  "timestamp": 1760270400
}
```

---

### 2. State Dump Start (IRCal)
```json
{
  "id": "1760270400-0002",
  "type": "state_dump",
  "server": "IRCal",
  "timestamp": 1760270400
}
```

---

### 3. IRCal - Kanał #ircnet (z topiciem)

#### 3.1. Channel Join (YOU)
```json
{
  "id": "1760270400-0003",
  "type": "channel_join",
  "server": "IRCal",
  "channel": "#ircnet",
  "nick": "kofany",
  "timestamp": 1760270400
}
```

#### 3.2. Topic
```json
{
  "id": "1760270400-0004",
  "type": "topic",
  "server": "IRCal",
  "channel": "#ircnet",
  "nick": "admin",
  "text": "Welcome to #ircnet | IRC since 1988 | Rules: https://ircnet.org/rules",
  "timestamp": 1760270395
}
```

#### 3.3. Nicklist (kompletna lista)
```json
{
  "id": "1760270400-0005",
  "type": "nicklist",
  "server": "IRCal",
  "channel": "#ircnet",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"alice\",\"prefix\":\"@\"},{\"nick\":\"bob\",\"prefix\":\"+\"},{\"nick\":\"charlie\",\"prefix\":\"\"},{\"nick\":\"david\",\"prefix\":\"\"},{\"nick\":\"eve\",\"prefix\":\"\"}]",
  "timestamp": 1760270400
}
```

**Uwaga:** `text` zawiera **JSON array jako string**. Frontend musi:
1. Parsować `text` jako JSON: `JSON.parse(msg.text)`
2. Iterować po array i dodawać użytkowników

---

### 4. IRCal - Kanał #irpg (bez topicu)

#### 4.1. Channel Join (YOU)
```json
{
  "id": "1760270400-0006",
  "type": "channel_join",
  "server": "IRCal",
  "channel": "#irpg",
  "nick": "kofany",
  "timestamp": 1760270400
}
```

#### 4.2. Nicklist
```json
{
  "id": "1760270400-0007",
  "type": "nicklist",
  "server": "IRCal",
  "channel": "#irpg",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"GameBot\",\"prefix\":\"@\"},{\"nick\":\"player1\",\"prefix\":\"+\"},{\"nick\":\"player2\",\"prefix\":\"\"},{\"nick\":\"spectator\",\"prefix\":\"\"}]",
  "timestamp": 1760270400
}
```

---

### 5. State Dump Start (IRCnet)
```json
{
  "id": "1760270400-0008",
  "type": "state_dump",
  "server": "IRCnet",
  "timestamp": 1760270400
}
```

---

### 6. IRCnet - Kanał #polska (z długim topiciem)

#### 6.1. Channel Join (YOU)
```json
{
  "id": "1760270400-0009",
  "type": "channel_join",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "kofany",
  "timestamp": 1760270400
}
```

#### 6.2. Topic
```json
{
  "id": "1760270400-0010",
  "type": "topic",
  "server": "IRCnet",
  "channel": "#polska",
  "nick": "oper",
  "text": "Witamy na #polska | Polski kanał na IRCnet | Szanuj innych | Off-topic: #polska-offtopic",
  "timestamp": 1760270350
}
```

#### 6.3. Nicklist (więcej userów)
```json
{
  "id": "1760270400-0011",
  "type": "nicklist",
  "server": "IRCnet",
  "channel": "#polska",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"oper\",\"prefix\":\"@\"},{\"nick\":\"moderator\",\"prefix\":\"@\"},{\"nick\":\"helper\",\"prefix\":\"+\"},{\"nick\":\"user1\",\"prefix\":\"\"},{\"nick\":\"user2\",\"prefix\":\"\"},{\"nick\":\"user3\",\"prefix\":\"\"},{\"nick\":\"guest\",\"prefix\":\"\"}]",
  "timestamp": 1760270400
}
```

---

### 7. IRCnet - Kanał #test (tylko ops)

#### 7.1. Channel Join (YOU)
```json
{
  "id": "1760270400-0012",
  "type": "channel_join",
  "server": "IRCnet",
  "channel": "#test",
  "nick": "kofany",
  "timestamp": 1760270400
}
```

#### 7.2. Topic
```json
{
  "id": "1760270400-0013",
  "type": "topic",
  "server": "IRCnet",
  "channel": "#test",
  "nick": "kofany",
  "text": "Testing area - experimental features only",
  "timestamp": 1760270380
}
```

#### 7.3. Nicklist (mało osób, wszyscy z op)
```json
{
  "id": "1760270400-0014",
  "type": "nicklist",
  "server": "IRCnet",
  "channel": "#test",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"admin\",\"prefix\":\"@\"},{\"nick\":\"testbot\",\"prefix\":\"@\"}]",
  "timestamp": 1760270400
}
```

---

### 8. IRCnet - Kanał #help (bez topicu, mieszane moды)

#### 8.1. Channel Join (YOU)
```json
{
  "id": "1760270400-0015",
  "type": "channel_join",
  "server": "IRCnet",
  "channel": "#help",
  "nick": "kofany",
  "timestamp": 1760270400
}
```

#### 8.2. Nicklist (różne prefixy)
```json
{
  "id": "1760270400-0016",
  "type": "nicklist",
  "server": "IRCnet",
  "channel": "#help",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"\"},{\"nick\":\"HelpBot\",\"prefix\":\"@\"},{\"nick\":\"volunteer1\",\"prefix\":\"+\"},{\"nick\":\"volunteer2\",\"prefix\":\"+\"},{\"nick\":\"newbie1\",\"prefix\":\"\"},{\"nick\":\"newbie2\",\"prefix\":\"\"},{\"nick\":\"lurker\",\"prefix\":\"\"}]",
  "timestamp": 1760270400
}
```

---

## Podsumowanie Struktury

### Dla każdej sieci:
1. **`state_dump`** - Marker rozpoczęcia dump dla serwera
2. **Dla każdego kanału (w kolejności):**
   - **`channel_join`** - YOU joined (tworzy kanał w UI)
   - **`topic`** (opcjonalnie) - Jeśli kanał ma topic
   - **`nicklist`** - **ZAWSZE** wysyłamy pełną listę

### Kluczowe punkty:

**1. `nicklist` zawiera JSON array jako STRING w polu `text`:**
```javascript
// ❌ BŁĄD - próba bezpośredniego użycia
channel.users = msg.text;  // To jest string, nie array!

// ✅ POPRAWNIE
const users = JSON.parse(msg.text);  // Teraz to array
users.forEach(user => {
  channel.addUser(user.nick, user.prefix);
});
```

**2. Prefix mapping:**
```javascript
const PREFIX_MAP = {
  '@': 'op',      // operator
  '+': 'voice',   // voice
  '%': 'halfop',  // half-op (rzadko)
  '': 'normal'    // zwykły user
};
```

**3. Kolejność jest GWARANTOWANA:**
- Najpierw `channel_join` (tworzy kanał)
- Potem `topic` (jeśli jest)
- Na końcu `nicklist` (ZAWSZE)

**4. Brak `nicklist` = bug:**
- Jeśli kanał nie dostał `nicklist`, to błąd serwera
- Każdy kanał MUSI dostać nicklist w state dump

---

## Format Nicklist - Szczegóły

### Przykładowy JSON array (przed zamianą na string):
```json
[
  {
    "nick": "kofany",
    "prefix": "@"
  },
  {
    "nick": "alice",
    "prefix": "@"
  },
  {
    "nick": "bob",
    "prefix": "+"
  },
  {
    "nick": "charlie",
    "prefix": ""
  }
]
```

### Po zamianie na string (w `text`):
```json
"[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"alice\",\"prefix\":\"@\"},{\"nick\":\"bob\",\"prefix\":\"+\"},{\"nick\":\"charlie\",\"prefix\":\"\"}]"
```

### Frontend MUSI:
```javascript
function handleNicklist(msg) {
  // 1. Znajdź kanał
  const channel = findChannel(msg.server, msg.channel);
  if (!channel) {
    console.error('Channel not found:', msg.channel);
    return;
  }

  // 2. Parsuj JSON array
  let users;
  try {
    users = JSON.parse(msg.text);
  } catch (e) {
    console.error('Failed to parse nicklist:', e);
    return;
  }

  // 3. Wyczyść starą listę
  channel.clearUsers();

  // 4. Dodaj wszystkich userów
  users.forEach(user => {
    channel.addUser({
      nick: user.nick,
      prefix: user.prefix,
      mode: prefixToMode(user.prefix)
    });
  });

  // 5. Posortuj (opcjonalnie)
  channel.sortUsers();

  console.log(`Loaded ${users.length} users for ${msg.channel}`);
}

function prefixToMode(prefix) {
  const modes = {
    '@': 'op',
    '+': 'voice',
    '%': 'halfop',
    '': 'normal'
  };
  return modes[prefix] || 'normal';
}
```

---

## Debugging - Co sprawdzić?

### 1. Czy `nicklist` dochodzi do frontendu?
```javascript
// W WebSocket message handler
case 'nicklist':
  console.log('NICKLIST RECEIVED:', {
    server: msg.server,
    channel: msg.channel,
    textLength: msg.text.length,
    firstChars: msg.text.substring(0, 50)
  });
  handleNicklist(msg);
  break;
```

### 2. Czy parsing działa?
```javascript
function handleNicklist(msg) {
  console.log('Parsing nicklist for', msg.channel);
  console.log('Raw text:', msg.text);

  try {
    const users = JSON.parse(msg.text);
    console.log('Parsed users:', users.length, users);
    // ... reszta
  } catch (e) {
    console.error('PARSING FAILED:', e);
    console.error('Text was:', msg.text);
  }
}
```

### 3. Czy kanał istnieje?
```javascript
const channel = findChannel(msg.server, msg.channel);
if (!channel) {
  console.error('CHANNEL NOT FOUND:', msg.server, msg.channel);
  console.error('Available channels:', listAllChannels());
  return;
}
```

### 4. Czy users są dodawani?
```javascript
users.forEach(user => {
  console.log('Adding user:', user.nick, 'with prefix:', user.prefix);
  channel.addUser(user);
  console.log('Channel now has', channel.users.length, 'users');
});
```

---

## Typowe Błędy Frontendu

### ❌ BŁĄD 1: Traktowanie `text` jako array
```javascript
// ŹLE!
channel.users = msg.text;
msg.text.forEach(user => ...);  // msg.text to STRING!
```

### ❌ BŁĄD 2: Tworzenie kanału DOPIERO przy nicklist
```javascript
// ŹLE! Kanał powinien być utworzony przy channel_join
case 'nicklist':
  const channel = createChannel(msg.channel);  // Za późno!
```

### ❌ BŁĄD 3: Nie czyszczenie starej listy
```javascript
// ŹLE! Dodajemy do starej listy
users.forEach(user => channel.addUser(user));
// POPRAWNIE: Najpierw wyczyść
channel.clearUsers();
users.forEach(user => channel.addUser(user));
```

### ❌ BŁĄD 4: Ignorowanie pustego prefix
```javascript
// ŹLE!
if (user.prefix) {  // Pomija userów bez prefix!
  channel.addUser(user);
}

// POPRAWNIE:
channel.addUser(user);  // Zawsze dodaj, prefix może być ""
```

---

## Sprawdzenie w irssi

Możesz przetestować co faktycznie wysyłamy:

### 1. W drugim terminalu:
```bash
websocat ws://localhost:9001
```

### 2. Wyślij auth + sync:
```json
{"type":"auth","password":"your-password"}
{"type":"sync_server","server":"*"}
```

### 3. Powinno przyjść:
```
{"type":"auth_ok",...}
{"type":"state_dump","server":"IRCal",...}
{"type":"channel_join","server":"IRCal","channel":"#ircnet",...}
{"type":"topic","server":"IRCal","channel":"#ircnet",...}
{"type":"nicklist","server":"IRCal","channel":"#ircnet","text":"[{...}]",...}
...
```

**Sprawdź czy `nicklist` faktycznie jest wysyłany!**

---

## Podsumowanie dla Agenta Frontendu

**Gdy przychodzi `channel_join`:**
1. Utwórz kanał w UI
2. Ustaw status: "oczekiwanie na dane"

**Gdy przychodzi `topic`:**
1. Znajdź kanał
2. Ustaw topic
3. Wyświetl topic

**Gdy przychodzi `nicklist`:**
1. Znajdź kanał (MUSI istnieć - stworzony przez `channel_join`)
2. Parsuj `msg.text` jako JSON array
3. Wyczyść starą listę użytkowników
4. Iteruj po array i dodaj każdego usera z jego prefix
5. Posortuj listę (op → voice → normal)
6. Ustaw status kanału: "gotowy"

**Jeśli kanał jest pusty:**
- ✅ Przyszło `channel_join`?
- ✅ Przyszło `nicklist`?
- ✅ `msg.text` zawiera JSON array?
- ✅ Parsing się udał?
- ✅ `channel.addUser()` jest wywoływane?
- ✅ Kanał istnieje w momencie dodawania?

---

**Data:** 2025-10-13
**Autor:** kofany + Claude
**Przeznaczenie:** Debug frontendu The Lounge
