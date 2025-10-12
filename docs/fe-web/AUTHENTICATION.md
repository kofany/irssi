# fe-web WebSocket Authentication

## Konfiguracja hasła w irssi

W irssi ustaw hasło dla fe-web:

```
/SET fe_web_password tajnehaslo123
/SAVE
```

Jeśli hasło nie jest ustawione, fe-web wyświetli ostrzeżenie ale pozwoli na połączenie (tylko do testów!).

## Autentykacja z poziomu klienta (The Lounge)

### Metoda: Query Parameter (ZALECANA)

Klient powinien dodać hasło do URL WebSocket jako parametr query:

```javascript
const password = "tajnehaslo123";
const wsUrl = `ws://localhost:9001/?password=${encodeURIComponent(password)}`;
const ws = new WebSocket(wsUrl);
```

### Przykład pełnego URL:

```
ws://localhost:9001/?password=tajnehaslo123
```

### Obsługa błędów autentykacji

Jeśli hasło jest nieprawidłowe lub nie zostało podane, serwer:

1. **Odrzuci połączenie** z kodem HTTP 401 Unauthorized
2. **Wyśle odpowiedź**:
   ```
   HTTP/1.1 401 Unauthorized
   Content-Type: text/plain
   Content-Length: 13
   
   Unauthorized
   ```
3. **Zamknie połączenie**

### Implementacja w The Lounge

#### Krok 1: Dodaj pole hasła w konfiguracji

W pliku konfiguracyjnym The Lounge dodaj pole dla hasła fe-web:

```json
{
  "irssi_fe_web": {
    "host": "localhost",
    "port": 9001,
    "password": "tajnehaslo123"
  }
}
```

#### Krok 2: Zbuduj URL z hasłem

```javascript
function connectToIrssi(config) {
  const { host, port, password } = config.irssi_fe_web;
  
  // Zbuduj URL z hasłem w query string
  let wsUrl = `ws://${host}:${port}/`;
  
  if (password && password.length > 0) {
    wsUrl += `?password=${encodeURIComponent(password)}`;
  }
  
  const ws = new WebSocket(wsUrl);
  
  ws.on('open', () => {
    console.log('Connected to irssi fe-web');
  });
  
  ws.on('error', (error) => {
    console.error('WebSocket error:', error);
  });
  
  ws.on('close', (code, reason) => {
    if (code === 1002) {
      console.error('Authentication failed - check password');
    }
  });
  
  return ws;
}
```

#### Krok 3: Obsługa błędów autentykacji

```javascript
ws.on('close', (code, reason) => {
  switch (code) {
    case 1002: // Protocol error (401 Unauthorized)
      console.error('Authentication failed - invalid password');
      // Pokaż użytkownikowi komunikat o błędnym haśle
      showError('Invalid irssi fe-web password');
      break;
    case 1000: // Normal closure
      console.log('Connection closed normally');
      break;
    default:
      console.error(`Connection closed with code ${code}: ${reason}`);
  }
});
```

## Bezpieczeństwo

### ⚠️ WAŻNE - Używaj SSL/TLS w produkcji!

Hasło w query string jest przesyłane w **plain text**. W produkcji **ZAWSZE** używaj:

1. **WebSocket Secure (wss://)** zamiast ws://
2. **Tunel SSH** lub **VPN** jeśli nie masz SSL
3. **Silne hasło** (min. 16 znaków, losowe)

### Przykład z SSL:

```javascript
const wsUrl = `wss://irssi.example.com:9001/?password=${password}`;
```

### Generowanie silnego hasła:

W irssi:

```
/SET fe_web_password $(openssl rand -base64 24)
/SAVE
```

Lub w JavaScript:

```javascript
const crypto = require('crypto');
const password = crypto.randomBytes(24).toString('base64');
```

## Testowanie autentykacji

### Test 1: Połączenie bez hasła (gdy hasło jest wymagane)

```bash
wscat -c "ws://localhost:9001/"
```

Oczekiwany wynik:
```
error: Unexpected server response: 401
```

### Test 2: Połączenie z prawidłowym hasłem

```bash
wscat -c "ws://localhost:9001/?password=tajnehaslo123"
```

Oczekiwany wynik:
```
Connected (press CTRL+C to quit)
< {"id":"...","type":"auth_ok","timestamp":...}
```

### Test 3: Połączenie z błędnym hasłem

```bash
wscat -c "ws://localhost:9001/?password=zlehaslo"
```

Oczekiwany wynik:
```
error: Unexpected server response: 401
```

## Logi w irssi

Po stronie irssi możesz monitorować próby autentykacji:

```
/SET fe_web_debug ON
```

Przykładowe logi:

```
fe-web: [1706198400-0001] Processing handshake, buffer len: 245
fe-web: Password verified successfully
fe-web: [1706198400-0001] Handshake completed successfully
```

Lub w przypadku błędu:

```
fe-web: [1706198400-0002] Processing handshake, buffer len: 230
fe-web: Invalid password!
fe-web: [1706198400-0002] Authentication failed - closing connection
```

## Podsumowanie dla developera The Lounge

1. **Dodaj pole `password`** w konfiguracji połączenia do irssi
2. **Zbuduj URL** z hasłem: `ws://host:port/?password=XXX`
3. **Obsłuż błąd 401** - pokaż użytkownikowi komunikat o błędnym haśle
4. **W produkcji używaj wss://** dla bezpieczeństwa
5. **Zakoduj hasło** przez `encodeURIComponent()` przed dodaniem do URL

## Przykład kompletnej implementacji

```javascript
class IrssiFeWebClient {
  constructor(config) {
    this.config = config;
    this.ws = null;
  }
  
  connect() {
    const { host, port, password, ssl } = this.config;
    const protocol = ssl ? 'wss' : 'ws';
    
    let url = `${protocol}://${host}:${port}/`;
    
    if (password) {
      url += `?password=${encodeURIComponent(password)}`;
    }
    
    this.ws = new WebSocket(url);
    
    this.ws.on('open', () => this.onOpen());
    this.ws.on('message', (data) => this.onMessage(data));
    this.ws.on('error', (error) => this.onError(error));
    this.ws.on('close', (code, reason) => this.onClose(code, reason));
  }
  
  onOpen() {
    console.log('Connected to irssi fe-web');
  }
  
  onMessage(data) {
    const message = JSON.parse(data);
    
    if (message.type === 'auth_ok') {
      console.log('Authentication successful');
      // Wyślij sync_server
      this.send({ type: 'sync_server', server: '*' });
    }
    
    // Obsłuż inne typy wiadomości...
  }
  
  onError(error) {
    console.error('WebSocket error:', error);
  }
  
  onClose(code, reason) {
    if (code === 1002) {
      console.error('Authentication failed - check password');
      throw new Error('Invalid irssi fe-web password');
    }
    console.log(`Connection closed: ${code} ${reason}`);
  }
  
  send(message) {
    this.ws.send(JSON.stringify(message));
  }
}

// Użycie:
const client = new IrssiFeWebClient({
  host: 'localhost',
  port: 9001,
  password: 'tajnehaslo123',
  ssl: false
});

client.connect();
```

