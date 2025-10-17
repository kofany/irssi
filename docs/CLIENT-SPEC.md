# Irssi FE-Web Client Implementation Guide

Guide for implementing web clients that connect to the FE-Web module.

## Quick Start

### 1. Establish Connection

```javascript
const ws = new WebSocket('wss://localhost:9001/?password=your-password');

ws.onopen = () => {
    console.log('Connected!');
};

ws.onmessage = async (event) => {
    // Decrypt and parse message
    const data = await event.data.arrayBuffer();
    const decrypted = await decrypt(data);
    const message = JSON.parse(decrypted);
    handleMessage(message);
};
```

### 2. Handle Authentication

```javascript
ws.onmessage = async (event) => {
    const message = await decryptMessage(event.data);
    
    if (message.type === 'auth_ok') {
        console.log('Authenticated!');
        // Request initial state
        sendMessage({ type: 'sync_server', server: '*' });
    }
};
```

### 3. Process Messages

```javascript
function handleMessage(msg) {
    switch (msg.type) {
        case 'message':
            displayMessage(msg);
            break;
        case 'channel_join':
            addChannel(msg.server, msg.target);
            break;
        case 'nicklist':
            updateNicklist(msg.server, msg.target, JSON.parse(msg.text));
            break;
        // ... handle other types
    }
}
```

## Complete Example

### WebSocket Client with Encryption

```javascript
class IrssiWebClient {
    constructor(url, password) {
        this.url = url;
        this.password = password;
        this.ws = null;
        this.encryptionKey = null;
        this.connected = false;
    }

    async connect() {
        // Derive encryption key from password
        this.encryptionKey = await this.deriveKey(this.password);
        
        // Connect WebSocket
        this.ws = new WebSocket(`${this.url}/?password=${this.password}`);
        this.ws.binaryType = 'arraybuffer';
        
        this.ws.onopen = () => this.handleOpen();
        this.ws.onmessage = (e) => this.handleMessage(e);
        this.ws.onclose = () => this.handleClose();
        this.ws.onerror = (e) => this.handleError(e);
    }

    async deriveKey(password) {
        const encoder = new TextEncoder();
        const passwordBuffer = encoder.encode(password);
        const salt = encoder.encode('irssi-fe-web-v1');
        
        // Import password as key material
        const keyMaterial = await crypto.subtle.importKey(
            'raw',
            passwordBuffer,
            'PBKDF2',
            false,
            ['deriveBits', 'deriveKey']
        );
        
        // Derive 256-bit key
        return await crypto.subtle.deriveKey(
            {
                name: 'PBKDF2',
                salt: salt,
                iterations: 10000,
                hash: 'SHA-256'
            },
            keyMaterial,
            { name: 'AES-GCM', length: 256 },
            false,
            ['encrypt', 'decrypt']
        );
    }

    async decrypt(ciphertext) {
        if (!this.encryptionKey) {
            throw new Error('Encryption key not initialized');
        }
        
        const view = new Uint8Array(ciphertext);
        
        // Extract IV (12 bytes), ciphertext, and tag (16 bytes)
        const iv = view.slice(0, 12);
        const encrypted = view.slice(12);
        
        try {
            const decrypted = await crypto.subtle.decrypt(
                { name: 'AES-GCM', iv: iv },
                this.encryptionKey,
                encrypted
            );
            
            const decoder = new TextDecoder();
            return decoder.decode(decrypted);
        } catch (e) {
            console.error('Decryption failed:', e);
            return null;
        }
    }

    async encrypt(plaintext) {
        if (!this.encryptionKey) {
            throw new Error('Encryption key not initialized');
        }
        
        const encoder = new TextEncoder();
        const data = encoder.encode(plaintext);
        
        // Generate random IV
        const iv = crypto.getRandomValues(new Uint8Array(12));
        
        const encrypted = await crypto.subtle.encrypt(
            { name: 'AES-GCM', iv: iv },
            this.encryptionKey,
            data
        );
        
        // Concatenate: IV || ciphertext+tag
        const result = new Uint8Array(12 + encrypted.byteLength);
        result.set(iv, 0);
        result.set(new Uint8Array(encrypted), 12);
        
        return result.buffer;
    }

    async handleMessage(event) {
        const json = await this.decrypt(event.data);
        if (!json) return;
        
        const message = JSON.parse(json);
        this.onMessage(message);
    }

    async sendMessage(obj) {
        const json = JSON.stringify(obj);
        const encrypted = await this.encrypt(json);
        this.ws.send(encrypted);
    }

    handleOpen() {
        console.log('WebSocket connected');
    }

    handleClose() {
        console.log('WebSocket closed');
        this.connected = false;
        // Implement reconnection logic
    }

    handleError(error) {
        console.error('WebSocket error:', error);
    }

    onMessage(message) {
        // Override this in your implementation
        console.log('Received:', message);
    }
}
```

### Usage Example

```javascript
const client = new IrssiWebClient('wss://localhost:9001', 'your-password');

client.onMessage = (msg) => {
    console.log('Message:', msg);
    
    if (msg.type === 'auth_ok') {
        // Authenticated, sync to all servers
        client.sendMessage({ type: 'sync_server', server: '*' });
    }
    else if (msg.type === 'message') {
        console.log(`[${msg.server}/${msg.target}] <${msg.nick}> ${msg.text}`);
    }
    else if (msg.type === 'state_dump') {
        console.log('Initial state received');
    }
};

client.connect();
```

## UI Implementation Examples

### Message Display

```javascript
function displayMessage(msg) {
    const messageEl = document.createElement('div');
    messageEl.className = 'message';
    
    // Format timestamp
    const time = new Date(msg.timestamp * 1000).toLocaleTimeString();
    
    // Build message HTML
    if (msg.is_own) {
        messageEl.innerHTML = `
            <span class="time">${time}</span>
            <span class="nick own">${escapeHtml(msg.nick)}</span>
            <span class="text">${escapeHtml(msg.text)}</span>
        `;
    } else {
        messageEl.innerHTML = `
            <span class="time">${time}</span>
            <span class="nick">${escapeHtml(msg.nick)}</span>
            <span class="text">${escapeHtml(msg.text)}</span>
        `;
    }
    
    // Add to channel view
    const channelView = getChannelView(msg.server, msg.target);
    channelView.appendChild(messageEl);
    channelView.scrollTop = channelView.scrollHeight;
}
```

### Channel List

```javascript
class ChannelList {
    constructor() {
        this.channels = new Map(); // key: "server:target"
    }

    handleMessage(msg) {
        switch (msg.type) {
            case 'channel_join':
                if (msg.is_own) {
                    this.addChannel(msg.server, msg.target);
                }
                break;
            case 'channel_part':
                if (msg.is_own) {
                    this.removeChannel(msg.server, msg.target);
                }
                break;
            case 'activity_update':
                this.updateActivity(msg.server, msg.target, msg.level);
                break;
        }
    }

    addChannel(server, target) {
        const key = `${server}:${target}`;
        if (this.channels.has(key)) return;
        
        const channel = {
            server: server,
            target: target,
            activity: 0,
            unread: 0
        };
        
        this.channels.set(key, channel);
        this.renderChannel(channel);
    }

    removeChannel(server, target) {
        const key = `${server}:${target}`;
        this.channels.delete(key);
        this.removeChannelElement(key);
    }

    updateActivity(server, target, level) {
        const key = `${server}:${target}`;
        const channel = this.channels.get(key);
        if (!channel) return;
        
        channel.activity = level;
        this.renderChannel(channel);
    }

    renderChannel(channel) {
        const el = document.getElementById(`channel-${channel.server}-${channel.target}`);
        if (!el) {
            // Create new element
            const newEl = this.createChannelElement(channel);
            document.getElementById('channel-list').appendChild(newEl);
        } else {
            // Update existing
            el.className = `channel activity-${channel.activity}`;
        }
    }

    createChannelElement(channel) {
        const el = document.createElement('div');
        el.id = `channel-${channel.server}-${channel.target}`;
        el.className = `channel activity-${channel.activity}`;
        el.textContent = channel.target;
        el.onclick = () => this.switchToChannel(channel);
        return el;
    }

    switchToChannel(channel) {
        // Show channel view
        showChannelView(channel.server, channel.target);
        
        // Mark as read
        client.sendMessage({
            type: 'mark_read',
            server: channel.server,
            target: channel.target
        });
    }
}
```

### Nicklist

```javascript
class Nicklist {
    constructor() {
        this.nicks = new Map(); // key: "server:channel"
    }

    handleMessage(msg) {
        switch (msg.type) {
            case 'nicklist':
                this.setNicklist(msg.server, msg.target, JSON.parse(msg.text));
                break;
            case 'nicklist_update':
                this.updateNick(msg.server, msg.target, msg.nick, msg.text);
                break;
        }
    }

    setNicklist(server, channel, nicks) {
        const key = `${server}:${channel}`;
        this.nicks.set(key, nicks);
        this.renderNicklist(server, channel);
    }

    updateNick(server, channel, nick, action) {
        const key = `${server}:${channel}`;
        let nicks = this.nicks.get(key) || [];
        
        if (action === 'add') {
            nicks.push({ nick: nick, prefix: '' });
        } else if (action === 'remove') {
            nicks = nicks.filter(n => n.nick !== nick);
        } else if (action.startsWith('+') || action.startsWith('-')) {
            // Mode change
            const nickObj = nicks.find(n => n.nick === nick);
            if (nickObj) {
                this.applyMode(nickObj, action);
            }
        }
        
        this.nicks.set(key, nicks);
        this.renderNicklist(server, channel);
    }

    applyMode(nickObj, mode) {
        if (mode === '+o') nickObj.prefix = '@';
        else if (mode === '-o') nickObj.prefix = nickObj.prefix.replace('@', '');
        else if (mode === '+v') nickObj.prefix += '+';
        else if (mode === '-v') nickObj.prefix = nickObj.prefix.replace('+', '');
        else if (mode === '+h') nickObj.prefix += '%';
        else if (mode === '-h') nickObj.prefix = nickObj.prefix.replace('%', '');
    }

    renderNicklist(server, channel) {
        const key = `${server}:${channel}`;
        const nicks = this.nicks.get(key) || [];
        
        // Sort by prefix, then alphabetically
        nicks.sort((a, b) => {
            const prefixOrder = { '@': 0, '%': 1, '+': 2, '': 3 };
            const orderA = prefixOrder[a.prefix.charAt(0)] || 3;
            const orderB = prefixOrder[b.prefix.charAt(0)] || 3;
            if (orderA !== orderB) return orderA - orderB;
            return a.nick.localeCompare(b.nick);
        });
        
        // Render
        const container = document.getElementById('nicklist');
        container.innerHTML = '';
        
        nicks.forEach(n => {
            const el = document.createElement('div');
            el.className = 'nick';
            el.textContent = `${n.prefix}${n.nick}`;
            container.appendChild(el);
        });
    }
}
```

### Input Handler

```javascript
class InputHandler {
    constructor(client) {
        this.client = client;
        this.currentServer = null;
        this.currentTarget = null;
    }

    handleInput(text) {
        if (!text) return;
        
        if (text.startsWith('/')) {
            // Command
            this.sendCommand(text);
        } else {
            // Message
            this.sendMessage(text);
        }
    }

    sendCommand(command) {
        this.client.sendMessage({
            type: 'command',
            command: command,
            server: this.currentServer
        });
    }

    sendMessage(text) {
        if (!this.currentTarget) {
            console.error('No target selected');
            return;
        }
        
        this.client.sendMessage({
            type: 'command',
            command: `/msg ${this.currentTarget} ${text}`,
            server: this.currentServer
        });
    }
}
```

### Network Management

```javascript
class NetworkManager {
    constructor(client) {
        this.client = client;
    }

    async loadNetworks() {
        return new Promise((resolve) => {
            const requestId = `net-list-${Date.now()}`;
            
            // Set up response handler
            const handler = (msg) => {
                if (msg.type === 'network_list_response' && 
                    msg.response_to === requestId) {
                    const networks = JSON.parse(msg.text);
                    this.client.removeListener(handler);
                    resolve(networks);
                }
            };
            this.client.addListener(handler);
            
            // Send request
            this.client.sendMessage({
                type: 'network_list',
                id: requestId
            });
        });
    }

    async addNetwork(name, nick, username, realname) {
        const requestId = `net-add-${Date.now()}`;
        
        return new Promise((resolve, reject) => {
            const handler = (msg) => {
                if (msg.type === 'command_result' && 
                    msg.response_to === requestId) {
                    const result = JSON.parse(msg.text);
                    this.client.removeListener(handler);
                    
                    if (result.success) {
                        resolve();
                    } else {
                        reject(new Error(result.message));
                    }
                }
            };
            this.client.addListener(handler);
            
            this.client.sendMessage({
                type: 'network_add',
                id: requestId,
                name: name,
                nick: nick,
                username: username,
                realname: realname
            });
        });
    }
}
```

## State Management

### Complete State Model

```javascript
class IrssiState {
    constructor() {
        this.servers = new Map();       // server_tag -> ServerState
        this.channels = new Map();      // "server:channel" -> ChannelState
        this.queries = new Map();       // "server:nick" -> QueryState
        this.nicklists = new Map();     // "server:channel" -> Nick[]
        this.activity = new Map();      // "server:target" -> level
    }

    handleMessage(msg) {
        switch (msg.type) {
            case 'server_status':
                this.updateServer(msg);
                break;
            case 'channel_join':
                this.addChannel(msg);
                break;
            case 'channel_part':
                this.removeChannel(msg);
                break;
            case 'query_opened':
                this.addQuery(msg);
                break;
            case 'query_closed':
                this.removeQuery(msg);
                break;
            case 'nicklist':
                this.setNicklist(msg);
                break;
            case 'nicklist_update':
                this.updateNicklist(msg);
                break;
            case 'activity_update':
                this.updateActivity(msg);
                break;
        }
    }

    updateServer(msg) {
        const server = this.servers.get(msg.server) || {
            tag: msg.server,
            connected: false,
            address: null,
            port: null
        };
        
        if (msg.text === 'connected') {
            server.connected = true;
            server.address = msg.extra?.address;
            server.port = msg.extra?.port;
        } else {
            server.connected = false;
        }
        
        this.servers.set(msg.server, server);
    }

    addChannel(msg) {
        const key = `${msg.server}:${msg.target}`;
        this.channels.set(key, {
            server: msg.server,
            name: msg.target,
            topic: null,
            joined: msg.is_own
        });
    }

    // ... implement other state methods
}
```

## Error Handling

```javascript
class RobustClient extends IrssiWebClient {
    constructor(url, password) {
        super(url, password);
        this.reconnectDelay = 1000;
        this.maxReconnectDelay = 30000;
        this.reconnectAttempts = 0;
    }

    handleClose() {
        console.log('Connection closed');
        this.connected = false;
        this.scheduleReconnect();
    }

    handleError(error) {
        console.error('WebSocket error:', error);
        this.scheduleReconnect();
    }

    scheduleReconnect() {
        const delay = Math.min(
            this.reconnectDelay * Math.pow(2, this.reconnectAttempts),
            this.maxReconnectDelay
        );
        
        console.log(`Reconnecting in ${delay}ms...`);
        
        setTimeout(() => {
            this.reconnectAttempts++;
            this.connect();
        }, delay);
    }

    async connect() {
        try {
            await super.connect();
            this.reconnectAttempts = 0; // Reset on successful connection
        } catch (error) {
            console.error('Connection failed:', error);
            this.scheduleReconnect();
        }
    }
}
```

## Performance Optimization

### Message Batching

```javascript
class BatchingClient extends IrssiWebClient {
    constructor(url, password) {
        super(url, password);
        this.messageQueue = [];
        this.batchTimer = null;
    }

    queueMessage(obj) {
        this.messageQueue.push(obj);
        
        if (!this.batchTimer) {
            this.batchTimer = setTimeout(() => {
                this.flushQueue();
            }, 50); // Batch messages within 50ms
        }
    }

    flushQueue() {
        if (this.messageQueue.length === 0) return;
        
        this.messageQueue.forEach(msg => {
            this.sendMessage(msg);
        });
        
        this.messageQueue = [];
        this.batchTimer = null;
    }
}
```

### Virtual Scrolling for Messages

```javascript
class VirtualMessageList {
    constructor(container, itemHeight) {
        this.container = container;
        this.itemHeight = itemHeight;
        this.messages = [];
        this.visibleStart = 0;
        this.visibleCount = 0;
        
        this.container.addEventListener('scroll', () => this.handleScroll());
        this.updateVisibleCount();
    }

    addMessage(msg) {
        this.messages.push(msg);
        this.render();
    }

    handleScroll() {
        this.visibleStart = Math.floor(this.container.scrollTop / this.itemHeight);
        this.render();
    }

    updateVisibleCount() {
        this.visibleCount = Math.ceil(this.container.clientHeight / this.itemHeight) + 2;
    }

    render() {
        const fragment = document.createDocumentFragment();
        const end = Math.min(this.visibleStart + this.visibleCount, this.messages.length);
        
        for (let i = this.visibleStart; i < end; i++) {
            const el = this.createMessageElement(this.messages[i]);
            el.style.position = 'absolute';
            el.style.top = `${i * this.itemHeight}px`;
            fragment.appendChild(el);
        }
        
        this.container.innerHTML = '';
        this.container.appendChild(fragment);
        this.container.style.height = `${this.messages.length * this.itemHeight}px`;
    }

    createMessageElement(msg) {
        // Implementation
    }
}
```

## Testing

### Unit Tests (Jest example)

```javascript
describe('IrssiWebClient', () => {
    test('derives encryption key correctly', async () => {
        const client = new IrssiWebClient('wss://test', 'password');
        const key = await client.deriveKey('test123');
        expect(key).toBeDefined();
    });

    test('encrypts and decrypts message', async () => {
        const client = new IrssiWebClient('wss://test', 'password');
        client.encryptionKey = await client.deriveKey('test123');
        
        const original = 'Hello, World!';
        const encrypted = await client.encrypt(original);
        const decrypted = await client.decrypt(encrypted);
        
        expect(decrypted).toBe(original);
    });

    test('parses JSON message', () => {
        const json = '{"type":"message","text":"Hello"}';
        const msg = JSON.parse(json);
        expect(msg.type).toBe('message');
        expect(msg.text).toBe('Hello');
    });
});
```

## Security Considerations

1. **Always use WSS** - Never use unencrypted WebSocket connections
2. **Validate server certificate** - Except for localhost/development
3. **Store password securely** - Use browser's secure storage APIs
4. **Clear sensitive data** - Overwrite password after key derivation
5. **Implement CSP** - Content Security Policy headers
6. **Sanitize HTML** - Escape all user-generated content

## License

GPL-2.0 - Same as Irssi
