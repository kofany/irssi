// IRC Protocol Implementation
export interface IRCMessage {
  prefix?: string
  command: string
  params: string[]
  trailing?: string
  raw: string
}

export class IRCConnection {
  private ws: WebSocket | null = null
  private callbacks: Map<string, Function[]> = new Map()
  private reconnectAttempts = 0
  private maxReconnectAttempts = 5
  private reconnectDelay = 1000

  constructor(
    private server: string,
    private port: number,
    private nick: string,
    private realName: string,
    private useSSL = true,
  ) {}

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        // Since browsers can't make direct TCP connections to IRC servers,
        // we need to use a WebSocket proxy service
        const proxyUrl = `wss://irc-ws.chat.twitch.tv/irc` // Example WebSocket IRC proxy

        // For demo purposes, we'll simulate a connection to show the UI working
        // In production, you'd need a proper WebSocket-to-IRC bridge service
        this.simulateConnection()
        resolve()
      } catch (error) {
        reject(error)
      }
    })
  }

  private simulateConnection() {
    // Simulate connection delay
    setTimeout(() => {
      this.emit("connected")
      this.emit("registered")

      // Simulate joining a channel
      setTimeout(() => {
        this.emit("join", {
          nick: this.nick,
          channel: "#polska",
        })

        // Simulate receiving names list
        this.emit("names", {
          channel: "#polska",
          names: ["@pjoter", "+Tamu", "snowek", "LySy", "doddy", "gon", "brudy"],
        })

        // Simulate some messages
        setTimeout(() => {
          this.emit("message", {
            nick: "pjoter",
            target: "#polska",
            text: "Witaj w kanale!",
            type: "message",
          })
        }, 1000)
      }, 500)
    }, 1000)
  }

  private connectViaProxy(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        // This would connect to a WebSocket-to-IRC proxy service
        // Examples: wss://irc.chat.twitch.tv/irc, or custom proxy
        const wsUrl = `wss://your-irc-proxy.com/connect?server=${this.server}&port=${this.port}&ssl=${this.useSSL}`

        this.ws = new WebSocket(wsUrl)

        this.ws.onopen = () => {
          this.reconnectAttempts = 0
          this.register()
          this.emit("connected")
          resolve()
        }

        this.ws.onmessage = (event) => {
          const message = this.parseMessage(event.data)
          this.handleMessage(message)
        }

        this.ws.onclose = () => {
          this.emit("disconnected")
          this.attemptReconnect()
        }

        this.ws.onerror = (error) => {
          this.emit("error", error)
          reject(error)
        }
      } catch (error) {
        reject(error)
      }
    })
  }

  disconnect() {
    if (this.ws) {
      this.ws.close()
      this.ws = null
    }
  }

  private register() {
    this.send(`NICK ${this.nick}`)
    this.send(`USER ${this.nick} 0 * :${this.realName}`)
  }

  private parseMessage(raw: string): IRCMessage {
    const message: IRCMessage = { command: "", params: [], raw }

    let line = raw.trim()

    // Parse prefix
    if (line.startsWith(":")) {
      const spaceIndex = line.indexOf(" ")
      message.prefix = line.substring(1, spaceIndex)
      line = line.substring(spaceIndex + 1)
    }

    // Parse trailing
    const colonIndex = line.indexOf(" :")
    if (colonIndex !== -1) {
      message.trailing = line.substring(colonIndex + 2)
      line = line.substring(0, colonIndex)
    }

    // Parse command and params
    const parts = line.split(" ")
    message.command = parts[0].toUpperCase()
    message.params = parts.slice(1)

    return message
  }

  private handleMessage(message: IRCMessage) {
    switch (message.command) {
      case "PING":
        this.send(`PONG :${message.trailing || message.params[0]}`)
        break

      case "001": // RPL_WELCOME
        this.emit("registered")
        break

      case "353": // RPL_NAMREPLY
        this.emit("names", {
          channel: message.params[2],
          names: message.trailing?.split(" ") || [],
        })
        break

      case "JOIN":
        this.emit("join", {
          nick: this.getNickFromPrefix(message.prefix),
          channel: message.params[0] || message.trailing,
        })
        break

      case "PART":
        this.emit("part", {
          nick: this.getNickFromPrefix(message.prefix),
          channel: message.params[0],
          reason: message.trailing,
        })
        break

      case "PRIVMSG":
        this.emit("message", {
          nick: this.getNickFromPrefix(message.prefix),
          target: message.params[0],
          text: message.trailing,
          type: message.trailing?.startsWith("\x01ACTION") ? "action" : "message",
        })
        break

      case "NOTICE":
        this.emit("notice", {
          nick: this.getNickFromPrefix(message.prefix),
          target: message.params[0],
          text: message.trailing,
        })
        break

      case "QUIT":
        this.emit("quit", {
          nick: this.getNickFromPrefix(message.prefix),
          reason: message.trailing,
        })
        break
    }

    this.emit("raw", message)
  }

  private getNickFromPrefix(prefix?: string): string {
    if (!prefix) return ""
    return prefix.split("!")[0]
  }

  private attemptReconnect() {
    if (this.reconnectAttempts < this.maxReconnectAttempts) {
      this.reconnectAttempts++
      setTimeout(() => {
        this.emit("reconnecting", this.reconnectAttempts)
        this.connect().catch(() => {})
      }, this.reconnectDelay * this.reconnectAttempts)
    }
  }

  send(data: string) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(data + "\r\n")
    } else {
      // Simulate command processing for demo
      console.log(`[IRC] Sending: ${data}`)
      this.handleSimulatedCommand(data)
    }
  }

  private handleSimulatedCommand(command: string) {
    const parts = command.split(" ")
    const cmd = parts[0].toUpperCase()

    switch (cmd) {
      case "JOIN":
        const channel = parts[1]
        setTimeout(() => {
          this.emit("join", {
            nick: this.nick,
            channel: channel,
          })
          this.emit("message", {
            nick: "system",
            target: channel,
            text: `Joined ${channel}`,
            type: "system",
          })
        }, 200)
        break

      case "PRIVMSG":
        const target = parts[1]
        const text = parts.slice(2).join(" ").substring(1) // Remove leading :
        setTimeout(() => {
          this.emit("message", {
            nick: this.nick,
            target: target,
            text: text,
            type: "message",
          })
        }, 100)
        break
    }
  }

  join(channel: string, key?: string) {
    this.send(`JOIN ${channel}${key ? ` ${key}` : ""}`)
  }

  part(channel: string, reason?: string) {
    this.send(`PART ${channel}${reason ? ` :${reason}` : ""}`)
  }

  privmsg(target: string, text: string) {
    this.send(`PRIVMSG ${target} :${text}`)
  }

  action(target: string, text: string) {
    this.send(`PRIVMSG ${target} :\x01ACTION ${text}\x01`)
  }

  notice(target: string, text: string) {
    this.send(`NOTICE ${target} :${text}`)
  }

  quit(reason?: string) {
    this.send(`QUIT${reason ? ` :${reason}` : ""}`)
  }

  on(event: string, callback: Function) {
    if (!this.callbacks.has(event)) {
      this.callbacks.set(event, [])
    }
    this.callbacks.get(event)!.push(callback)
  }

  off(event: string, callback: Function) {
    const callbacks = this.callbacks.get(event)
    if (callbacks) {
      const index = callbacks.indexOf(callback)
      if (index > -1) {
        callbacks.splice(index, 1)
      }
    }
  }

  private emit(event: string, data?: any) {
    const callbacks = this.callbacks.get(event)
    if (callbacks) {
      callbacks.forEach((callback) => callback(data))
    }
  }
}

// IRC Client Manager
export class IRCClientManager {
  private connections: Map<string, IRCConnection> = new Map()
  private callbacks: Map<string, Function[]> = new Map()

  createConnection(
    networkName: string,
    config: {
      server: string
      port: number
      nick: string
      realName: string
      useSSL?: boolean
    },
  ): IRCConnection {
    const connection = new IRCConnection(config.server, config.port, config.nick, config.realName, config.useSSL)

    // Forward all events with network context
    const events = [
      "connected",
      "disconnected",
      "registered",
      "message",
      "join",
      "part",
      "quit",
      "names",
      "notice",
      "error",
      "reconnecting",
    ]
    events.forEach((event) => {
      connection.on(event, (data: any) => {
        this.emit(event, { network: networkName, ...data })
      })
    })

    this.connections.set(networkName, connection)
    return connection
  }

  getConnection(networkName: string): IRCConnection | undefined {
    return this.connections.get(networkName)
  }

  removeConnection(networkName: string) {
    const connection = this.connections.get(networkName)
    if (connection) {
      connection.disconnect()
      this.connections.delete(networkName)
    }
  }

  on(event: string, callback: Function) {
    if (!this.callbacks.has(event)) {
      this.callbacks.set(event, [])
    }
    this.callbacks.get(event)!.push(callback)
  }

  private emit(event: string, data?: any) {
    const callbacks = this.callbacks.get(event)
    if (callbacks) {
      callbacks.forEach((callback) => callback(data))
    }
  }
}
