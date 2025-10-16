# Network/Server Management - Code Snippets & Examples

## Complete Code Snippets for The Lounge Integration

### 1. WebSocket Client Setup

```javascript
// thelounge/src/irssi-websocket.js
class IrssiWebSocket {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.pendingRequests = new Map();
    
    this.ws.on('message', (data) => {
      const msg = JSON.parse(data);
      this.handleMessage(msg);
    });
  }
  
  handleMessage(msg) {
    // Handle response messages
    if (msg.response_to) {
      const callback = this.pendingRequests.get(msg.response_to);
      if (callback) {
        callback(msg);
        this.pendingRequests.delete(msg.response_to);
      }
    }
    
    // Handle other message types
    switch(msg.type) {
      case 'network_list_response':
        this.emit('networks:list', msg.networks);
        break;
      case 'server_list_response':
        this.emit('servers:list', msg.servers);
        break;
      case 'command_result':
        this.handleCommandResult(msg);
        break;
    }
  }
  
  sendRequest(type, data, callback) {
    const id = this.generateUUID();
    const request = {
      type,
      id,
      ...data
    };
    
    this.pendingRequests.set(id, callback);
    this.ws.send(JSON.stringify(request));
    
    return id;
  }
  
  generateUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
      const r = Math.random() * 16 | 0;
      const v = c == 'x' ? r : (r & 0x3 | 0x8);
      return v.toString(16);
    });
  }
}
```

### 2. Network Management API Wrapper

```javascript
// thelounge/src/api/networks.js
class NetworkAPI {
  constructor(websocket) {
    this.ws = websocket;
  }
  
  async listNetworks() {
    return new Promise((resolve, reject) => {
      this.ws.sendRequest('network_list', {}, (response) => {
        if (response.type === 'network_list_response') {
          resolve(response.networks);
        } else {
          reject(new Error('Unexpected response type'));
        }
      });
      
      // Timeout after 5 seconds
      setTimeout(() => reject(new Error('Request timeout')), 5000);
    });
  }
  
  async addNetwork(network) {
    return new Promise((resolve, reject) => {
      this.ws.sendRequest('network_add', network, (response) => {
        if (response.type === 'command_result') {
          if (response.success) {
            resolve(response);
          } else {
            reject(new Error(response.message));
          }
        }
      });
      
      setTimeout(() => reject(new Error('Request timeout')), 5000);
    });
  }
  
  async removeNetwork(name) {
    return new Promise((resolve, reject) => {
      this.ws.sendRequest('network_remove', { name }, (response) => {
        if (response.type === 'command_result') {
          if (response.success) {
            resolve(response);
          } else {
            reject(new Error(response.message));
          }
        }
      });
      
      setTimeout(() => reject(new Error('Request timeout')), 5000);
    });
  }
  
  async listServers(network = null) {
    return new Promise((resolve, reject) => {
      this.ws.sendRequest('server_list', { network }, (response) => {
        if (response.type === 'server_list_response') {
          resolve(response.servers);
        } else {
          reject(new Error('Unexpected response type'));
        }
      });
      
      setTimeout(() => reject(new Error('Request timeout')), 5000);
    });
  }
  
  async addServer(server) {
    return new Promise((resolve, reject) => {
      this.ws.sendRequest('server_add', server, (response) => {
        if (response.type === 'command_result') {
          if (response.success) {
            resolve(response);
          } else {
            reject(new Error(response.message));
          }
        }
      });
      
      setTimeout(() => reject(new Error('Request timeout')), 5000);
    });
  }
  
  async removeServer(address, port, chatnet) {
    return new Promise((resolve, reject) => {
      this.ws.sendRequest('server_remove', { address, port, chatnet }, (response) => {
        if (response.type === 'command_result') {
          if (response.success) {
            resolve(response);
          } else {
            reject(new Error(response.message));
          }
        }
      });
      
      setTimeout(() => reject(new Error('Request timeout')), 5000);
    });
  }
}

module.exports = NetworkAPI;
```

### 3. React Component Example

```jsx
// thelounge/client/components/NetworkManager.jsx
import React, { useState, useEffect } from 'react';
import { useIrssi } from '../hooks/useIrssi';

function NetworkManager() {
  const { networkAPI } = useIrssi();
  const [networks, setNetworks] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  
  useEffect(() => {
    loadNetworks();
  }, []);
  
  async function loadNetworks() {
    try {
      setLoading(true);
      const data = await networkAPI.listNetworks();
      setNetworks(data);
      setError(null);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  }
  
  async function handleAddNetwork(networkData) {
    try {
      await networkAPI.addNetwork(networkData);
      await loadNetworks(); // Reload list
    } catch (err) {
      alert(`Failed to add network: ${err.message}`);
    }
  }
  
  async function handleRemoveNetwork(name) {
    if (!confirm(`Remove network "${name}"?`)) return;
    
    try {
      await networkAPI.removeNetwork(name);
      await loadNetworks(); // Reload list
    } catch (err) {
      alert(`Failed to remove network: ${err.message}`);
    }
  }
  
  if (loading) return <div>Loading networks...</div>;
  if (error) return <div>Error: {error}</div>;
  
  return (
    <div className="network-manager">
      <h2>IRC Networks</h2>
      
      <button onClick={() => setShowAddForm(true)}>
        Add Network
      </button>
      
      <div className="network-list">
        {networks.map(network => (
          <NetworkCard 
            key={network.name}
            network={network}
            onRemove={handleRemoveNetwork}
          />
        ))}
      </div>
      
      {showAddForm && (
        <AddNetworkForm 
          onSubmit={handleAddNetwork}
          onCancel={() => setShowAddForm(false)}
        />
      )}
    </div>
  );
}

function NetworkCard({ network, onRemove }) {
  return (
    <div className="network-card">
      <h3>{network.name}</h3>
      <dl>
        <dt>Nickname:</dt>
        <dd>{network.nick || '(default)'}</dd>
        
        <dt>SASL:</dt>
        <dd>{network.sasl_mechanism || 'Disabled'}</dd>
        
        <dt>User Mode:</dt>
        <dd>{network.usermode || '(default)'}</dd>
      </dl>
      
      <button onClick={() => onRemove(network.name)}>
        Remove
      </button>
    </div>
  );
}

function AddNetworkForm({ onSubmit, onCancel }) {
  const [formData, setFormData] = useState({
    name: '',
    nick: '',
    alternate_nick: '',
    username: '',
    realname: '',
    sasl_mechanism: '',
    sasl_username: '',
    sasl_password: ''
  });
  
  function handleChange(e) {
    setFormData({
      ...formData,
      [e.target.name]: e.target.value
    });
  }
  
  function handleSubmit(e) {
    e.preventDefault();
    
    // Filter out empty fields
    const data = Object.entries(formData)
      .filter(([key, value]) => value !== '')
      .reduce((obj, [key, value]) => ({ ...obj, [key]: value }), {});
    
    onSubmit(data);
  }
  
  return (
    <form onSubmit={handleSubmit} className="add-network-form">
      <h3>Add Network</h3>
      
      <label>
        Network Name (required):
        <input 
          type="text" 
          name="name" 
          value={formData.name}
          onChange={handleChange}
          required 
        />
      </label>
      
      <label>
        Nickname:
        <input 
          type="text" 
          name="nick" 
          value={formData.nick}
          onChange={handleChange}
        />
      </label>
      
      <label>
        Alternate Nick:
        <input 
          type="text" 
          name="alternate_nick" 
          value={formData.alternate_nick}
          onChange={handleChange}
        />
      </label>
      
      <label>
        Username:
        <input 
          type="text" 
          name="username" 
          value={formData.username}
          onChange={handleChange}
        />
      </label>
      
      <label>
        Real Name:
        <input 
          type="text" 
          name="realname" 
          value={formData.realname}
          onChange={handleChange}
        />
      </label>
      
      <fieldset>
        <legend>SASL Authentication</legend>
        
        <label>
          Mechanism:
          <select 
            name="sasl_mechanism" 
            value={formData.sasl_mechanism}
            onChange={handleChange}
          >
            <option value="">None</option>
            <option value="PLAIN">PLAIN</option>
            <option value="EXTERNAL">EXTERNAL</option>
            <option value="SCRAM-SHA-256">SCRAM-SHA-256</option>
          </select>
        </label>
        
        {formData.sasl_mechanism && (
          <>
            <label>
              Username:
              <input 
                type="text" 
                name="sasl_username" 
                value={formData.sasl_username}
                onChange={handleChange}
              />
            </label>
            
            <label>
              Password:
              <input 
                type="password" 
                name="sasl_password" 
                value={formData.sasl_password}
                onChange={handleChange}
              />
            </label>
          </>
        )}
      </fieldset>
      
      <div className="form-actions">
        <button type="submit">Add Network</button>
        <button type="button" onClick={onCancel}>Cancel</button>
      </div>
    </form>
  );
}

export default NetworkManager;
```

### 4. Server Manager Component

```jsx
// thelounge/client/components/ServerManager.jsx
import React, { useState, useEffect } from 'react';
import { useIrssi } from '../hooks/useIrssi';

function ServerManager({ networkName }) {
  const { networkAPI } = useIrssi();
  const [servers, setServers] = useState([]);
  const [loading, setLoading] = useState(true);
  
  useEffect(() => {
    loadServers();
  }, [networkName]);
  
  async function loadServers() {
    try {
      setLoading(true);
      const data = await networkAPI.listServers(networkName);
      setServers(data);
    } catch (err) {
      console.error('Failed to load servers:', err);
    } finally {
      setLoading(false);
    }
  }
  
  async function handleAddServer(serverData) {
    try {
      await networkAPI.addServer({
        ...serverData,
        chatnet: networkName
      });
      await loadServers();
    } catch (err) {
      alert(`Failed to add server: ${err.message}`);
    }
  }
  
  async function handleRemoveServer(address, port) {
    if (!confirm(`Remove server ${address}:${port}?`)) return;
    
    try {
      await networkAPI.removeServer(address, port, networkName);
      await loadServers();
    } catch (err) {
      alert(`Failed to remove server: ${err.message}`);
    }
  }
  
  if (loading) return <div>Loading servers...</div>;
  
  return (
    <div className="server-manager">
      <h3>Servers for {networkName}</h3>
      
      <button onClick={() => setShowAddForm(true)}>
        Add Server
      </button>
      
      <table className="server-list">
        <thead>
          <tr>
            <th>Address</th>
            <th>Port</th>
            <th>TLS</th>
            <th>Auto</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {servers.map(server => (
            <tr key={`${server.address}:${server.port}`}>
              <td>{server.address}</td>
              <td>{server.port}</td>
              <td>{server.use_tls ? '✓' : '✗'}</td>
              <td>{server.autoconnect ? '✓' : '✗'}</td>
              <td>
                <button onClick={() => handleRemoveServer(server.address, server.port)}>
                  Remove
                </button>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
      
      {showAddForm && (
        <AddServerForm 
          networkName={networkName}
          onSubmit={handleAddServer}
          onCancel={() => setShowAddForm(false)}
        />
      )}
    </div>
  );
}

function AddServerForm({ networkName, onSubmit, onCancel }) {
  const [formData, setFormData] = useState({
    address: '',
    port: 6697,
    autoconnect: false,
    use_tls: true,
    tls_verify: true
  });
  
  function handleSubmit(e) {
    e.preventDefault();
    onSubmit(formData);
  }
  
  return (
    <form onSubmit={handleSubmit} className="add-server-form">
      <h4>Add Server to {networkName}</h4>
      
      <label>
        Address (required):
        <input 
          type="text" 
          name="address" 
          value={formData.address}
          onChange={(e) => setFormData({...formData, address: e.target.value})}
          required 
          placeholder="irc.example.com"
        />
      </label>
      
      <label>
        Port:
        <input 
          type="number" 
          name="port" 
          value={formData.port}
          onChange={(e) => setFormData({...formData, port: parseInt(e.target.value)})}
          min="1"
          max="65535"
        />
      </label>
      
      <label>
        <input 
          type="checkbox" 
          name="use_tls"
          checked={formData.use_tls}
          onChange={(e) => setFormData({...formData, use_tls: e.target.checked})}
        />
        Use TLS/SSL
      </label>
      
      {formData.use_tls && (
        <label>
          <input 
            type="checkbox" 
            name="tls_verify"
            checked={formData.tls_verify}
            onChange={(e) => setFormData({...formData, tls_verify: e.target.checked})}
          />
          Verify TLS Certificate
        </label>
      )}
      
      <label>
        <input 
          type="checkbox" 
          name="autoconnect"
          checked={formData.autoconnect}
          onChange={(e) => setFormData({...formData, autoconnect: e.target.checked})}
        />
        Auto-connect on startup
      </label>
      
      <div className="form-actions">
        <button type="submit">Add Server</button>
        <button type="button" onClick={onCancel}>Cancel</button>
      </div>
    </form>
  );
}

export default ServerManager;
```

### 5. Complete Usage Example

```javascript
// thelounge/src/index.js
const IrssiWebSocket = require('./irssi-websocket');
const NetworkAPI = require('./api/networks');

// Initialize WebSocket connection
const ws = new IrssiWebSocket('ws://localhost:8080');

// Create API wrapper
const networkAPI = new NetworkAPI(ws);

// Example: List all networks
async function listAllNetworks() {
  try {
    const networks = await networkAPI.listNetworks();
    console.log('Networks:', networks);
    
    // List servers for each network
    for (const network of networks) {
      const servers = await networkAPI.listServers(network.name);
      console.log(`Servers for ${network.name}:`, servers);
    }
  } catch (error) {
    console.error('Error:', error);
  }
}

// Example: Add a new network with server
async function addLiberaChat() {
  try {
    // Add network
    await networkAPI.addNetwork({
      name: 'Libera.Chat',
      nick: 'myuser',
      alternate_nick: 'myuser_',
      sasl_mechanism: 'PLAIN',
      sasl_username: 'myaccount',
      sasl_password: 'mypassword'
    });
    
    // Add server
    await networkAPI.addServer({
      address: 'irc.libera.chat',
      port: 6697,
      chatnet: 'Libera.Chat',
      autoconnect: true,
      use_tls: true,
      tls_verify: true
    });
    
    console.log('Libera.Chat network and server added successfully!');
  } catch (error) {
    console.error('Error:', error);
  }
}

// Example: Remove network and all its servers
async function removeNetwork(name) {
  try {
    await networkAPI.removeNetwork(name);
    console.log(`Network ${name} removed successfully!`);
  } catch (error) {
    console.error('Error:', error);
  }
}

// Run examples
(async () => {
  await listAllNetworks();
  await addLiberaChat();
  await listAllNetworks(); // Should now include Libera.Chat
})();
```

## Testing Scripts

### Test Script 1: Basic Connectivity

```javascript
// test-connection.js
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:8080');

ws.on('open', () => {
  console.log('Connected to irssi fe-web');
  
  // Send network list request
  const request = {
    type: 'network_list',
    id: 'test-' + Date.now()
  };
  
  console.log('Sending:', JSON.stringify(request));
  ws.send(JSON.stringify(request));
});

ws.on('message', (data) => {
  console.log('Received:', data.toString());
  
  const msg = JSON.parse(data);
  if (msg.type === 'network_list_response') {
    console.log(`Found ${msg.networks.length} networks`);
    msg.networks.forEach(net => {
      console.log(`  - ${net.name} (nick: ${net.nick || 'default'})`);
    });
    ws.close();
  }
});

ws.on('error', (error) => {
  console.error('Error:', error);
});

ws.on('close', () => {
  console.log('Connection closed');
});
```

### Test Script 2: CRUD Operations

```javascript
// test-crud.js
const WebSocket = require('ws');

class IrssiTester {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.requestId = 0;
    this.callbacks = new Map();
    
    this.ws.on('message', (data) => {
      const msg = JSON.parse(data);
      const callback = this.callbacks.get(msg.response_to);
      if (callback) {
        callback(msg);
        this.callbacks.delete(msg.response_to);
      }
    });
  }
  
  sendRequest(type, data) {
    return new Promise((resolve, reject) => {
      const id = 'test-' + (++this.requestId);
      const request = { type, id, ...data };
      
      this.callbacks.set(id, resolve);
      this.ws.send(JSON.stringify(request));
      
      setTimeout(() => reject(new Error('Timeout')), 5000);
    });
  }
  
  async runTests() {
    console.log('Starting CRUD tests...\n');
    
    // Test 1: List networks
    console.log('1. Listing networks...');
    let response = await this.sendRequest('network_list', {});
    console.log(`   Found ${response.networks.length} networks`);
    
    // Test 2: Add network
    console.log('\n2. Adding test network...');
    response = await this.sendRequest('network_add', {
      name: 'TestNetwork',
      nick: 'testuser',
      sasl_mechanism: 'PLAIN',
      sasl_username: 'test',
      sasl_password: 'password'
    });
    console.log(`   Result: ${response.message}`);
    
    // Test 3: Verify network was added
    console.log('\n3. Verifying network was added...');
    response = await this.sendRequest('network_list', {});
    const found = response.networks.find(n => n.name === 'TestNetwork');
    console.log(`   TestNetwork found: ${found ? 'YES' : 'NO'}`);
    if (found) {
      console.log(`   Nick: ${found.nick}`);
      console.log(`   SASL: ${found.sasl_mechanism}`);
    }
    
    // Test 4: Add server
    console.log('\n4. Adding test server...');
    response = await this.sendRequest('server_add', {
      address: 'irc.test.com',
      port: 6667,
      chatnet: 'TestNetwork',
      autoconnect: false,
      use_tls: false
    });
    console.log(`   Result: ${response.message}`);
    
    // Test 5: List servers
    console.log('\n5. Listing servers for TestNetwork...');
    response = await this.sendRequest('server_list', {
      network: 'TestNetwork'
    });
    console.log(`   Found ${response.servers.length} servers`);
    
    // Test 6: Remove server
    console.log('\n6. Removing test server...');
    response = await this.sendRequest('server_remove', {
      address: 'irc.test.com',
      port: 6667,
      chatnet: 'TestNetwork'
    });
    console.log(`   Result: ${response.message}`);
    
    // Test 7: Remove network
    console.log('\n7. Removing test network...');
    response = await this.sendRequest('network_remove', {
      name: 'TestNetwork'
    });
    console.log(`   Result: ${response.message}`);
    
    console.log('\n✓ All tests completed!');
    this.ws.close();
  }
}

const tester = new IrssiTester('ws://localhost:8080');
tester.ws.on('open', () => {
  tester.runTests().catch(err => {
    console.error('Test failed:', err);
    process.exit(1);
  });
});
```

## Error Handling Examples

```javascript
// Error handling patterns

async function robustNetworkOperation() {
  try {
    const networks = await networkAPI.listNetworks();
    return networks;
  } catch (error) {
    if (error.message.includes('timeout')) {
      // Retry once
      console.warn('Request timed out, retrying...');
      return await networkAPI.listNetworks();
    }
    
    if (error.message.includes('NETWORK_NOT_FOUND')) {
      console.error('Network does not exist');
      return null;
    }
    
    // Generic error
    console.error('Operation failed:', error);
    throw error;
  }
}

// Validate before sending
function validateNetwork(network) {
  if (!network.name || network.name.trim() === '') {
    throw new Error('Network name is required');
  }
  
  if (network.sasl_mechanism && !network.sasl_username) {
    throw new Error('SASL username required when SASL is enabled');
  }
  
  // More validation...
  return true;
}

async function safeAddNetwork(network) {
  try {
    validateNetwork(network);
    return await networkAPI.addNetwork(network);
  } catch (error) {
    // Handle specific errors
    if (error.message.includes('NETWORK_EXISTS')) {
      // Maybe update instead?
      console.warn('Network already exists, consider updating');
    }
    throw error;
  }
}
```

---

**Note**: These are complete working examples. Adapt to your specific needs and error handling requirements.
