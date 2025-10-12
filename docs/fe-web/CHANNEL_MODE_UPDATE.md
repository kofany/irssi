# Channel Mode Event - Breaking Change

## Data: 2025-10-12

## Zmiana w strukturze eventu `channel_mode`

### Stary format (DEPRECATED)
```json
{
  "id": "1760273492-0012",
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "text": "+o kfn",
  "timestamp": 1760273492
}
```

### Nowy format (CURRENT)
```json
{
  "id": "1760273492-0012",
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "timestamp": 1760273492,
  "extra": {
    "mode": "+o",
    "params": ["kfn"]
  }
}
```

## Dlaczego zmiana?

1. **Structured data > plain text** - zgodne z best practices dla JSON API
2. **Łatwiejsze parsowanie** - klient nie musi znać składni IRC MODE
3. **Elastyczność** - łatwo dodać nowe pola w przyszłości
4. **Zgodność z innymi protokołami** - WeeChat i inne używają podobnego podejścia

## Przykłady różnych typów mode

### User mode (+o, -o, +v, -v)
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+o",
    "params": ["kfn"]
  }
}
```

### Multiple user modes
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+oo",
    "params": ["alice", "bob"]
  }
}
```

### Channel limit (+l)
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+l",
    "params": ["100"]
  }
}
```

### Ban mode (+b)
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+b",
    "params": ["*!*@*.example.com"]
  }
}
```

### Channel key (+k)
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+k",
    "params": ["secret_password"]
  }
}
```

### Simple channel modes (no params)
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+nt",
    "params": []
  }
}
```

### Mixed modes
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#irc.al",
  "nick": "kofany",
  "extra": {
    "mode": "+o-v",
    "params": ["alice", "bob"]
  }
}
```

## Co zmienić w kliencie The Lounge?

### 1. Parser eventu channel_mode

**Stary kod (do usunięcia):**
```javascript
// Przykład - NIE UŻYWAJ
socket.on('channel_mode', (data) => {
  const modeText = data.text; // "+o kfn"
  // Parsowanie ręczne...
});
```

**Nowy kod:**
```javascript
socket.on('channel_mode', (data) => {
  const mode = data.extra.mode;      // "+o"
  const params = data.extra.params;  // ["kfn"]
  
  // Przykład użycia:
  if (mode.includes('o')) {
    const target = params[0];
    if (mode.startsWith('+')) {
      console.log(`${data.nick} gave op to ${target}`);
    } else {
      console.log(`${data.nick} removed op from ${target}`);
    }
  }
});
```

### 2. Wyświetlanie w UI

**Zalecane podejście:**
```javascript
function formatModeChange(data) {
  const { mode, params } = data.extra;
  const nick = data.nick;
  
  // Dla user modes (+o, +v, etc.)
  if (params.length > 0) {
    return `${nick} sets mode ${mode} ${params.join(' ')}`;
  }
  
  // Dla channel modes bez parametrów (+n, +t, etc.)
  return `${nick} sets mode ${mode}`;
}
```

### 3. Obsługa nicklist update

**WAŻNE:** Po zmianie user mode (np. +o/-o), serwer wysyła **DWA** eventy:

1. `channel_mode` - informacja kto i jaki mode ustawił
2. `nicklist` - zaktualizowana lista użytkowników z nowymi prefixami

**Przykład sekwencji:**
```json
// Event 1: Mode change
{
  "type": "channel_mode",
  "nick": "kofany",
  "extra": {
    "mode": "+o",
    "params": ["kfn"]
  }
}

// Event 2: Nicklist update (zaraz po)
{
  "type": "nicklist",
  "channel": "#irc.al",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"kfn\",\"prefix\":\"@\"}]"
}
```

**Implementacja w kliencie:**
```javascript
// 1. Wyświetl informację o zmianie mode
socket.on('channel_mode', (data) => {
  addMessage(data.channel, formatModeChange(data));
});

// 2. Zaktualizuj nicklist
socket.on('nicklist', (data) => {
  const users = JSON.parse(data.text);
  updateChannelNicklist(data.channel, users);
});
```

### 4. Mapowanie mode na opis

**Helper function:**
```javascript
const MODE_DESCRIPTIONS = {
  'o': 'operator',
  'v': 'voice',
  'h': 'halfop',
  'b': 'ban',
  'l': 'limit',
  'k': 'key',
  'n': 'no external messages',
  't': 'topic protection',
  'm': 'moderated',
  'i': 'invite only',
  's': 'secret',
  'p': 'private'
};

function describeModeChange(mode, params) {
  const action = mode[0]; // '+' or '-'
  const modeChar = mode[1];
  const description = MODE_DESCRIPTIONS[modeChar] || modeChar;
  
  if (params.length > 0) {
    return `${action === '+' ? 'gives' : 'removes'} ${description} ${action === '+' ? 'to' : 'from'} ${params.join(', ')}`;
  }
  
  return `${action === '+' ? 'sets' : 'removes'} ${description}`;
}
```

## Migracja

### Krok 1: Sprawdź obecność pola `extra`
```javascript
if (data.extra && data.extra.mode) {
  // Nowy format
  handleNewFormat(data);
} else if (data.text) {
  // Stary format (backward compatibility - opcjonalne)
  handleOldFormat(data);
}
```

### Krok 2: Usuń stary kod
Po weryfikacji że nowy format działa, usuń obsługę `data.text` dla `channel_mode`.

## Testowanie

### Test cases do sprawdzenia:

1. **User mode change:**
   - `/mode #channel +o nick`
   - `/mode #channel -o nick`
   - `/mode #channel +v nick`

2. **Multiple modes:**
   - `/mode #channel +oo alice bob`
   - `/mode #channel +o-v alice bob`

3. **Channel modes:**
   - `/mode #channel +l 100`
   - `/mode #channel +k password`
   - `/mode #channel +nt`
   - `/mode #channel -l`

4. **Ban modes:**
   - `/mode #channel +b *!*@*.example.com`
   - `/mode #channel -b *!*@*.example.com`

## Pytania?

Jeśli masz pytania lub napotkasz problemy, skontaktuj się z zespołem irssi fe-web.

## Changelog

- **2025-10-12 15:25** - Implementacja structured data dla channel_mode
- **2025-10-12 15:05** - Zmiana nick mode changed z channel_mode na nicklist

