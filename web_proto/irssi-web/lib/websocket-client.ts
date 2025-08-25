import type { IRCMessage } from "./types"

export class IRCWebSocketClient {
  private ws: WebSocket | null = null
  private reconnectAttempts = 0
  private maxReconnectAttempts = 5
  private reconnectDelay = 1000
  private messageHandlers: ((message: IRCMessage) => void)[] = []
  private connectionHandlers: ((connected: boolean) => void)[] = []

  constructor(private url = "ws://localhost:9001") {}

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        this.ws = new WebSocket(this.url)

        this.ws.onopen = () => {
          console.log("[IRC] WebSocket connected to", this.url)
          this.reconnectAttempts = 0
          this.connectionHandlers.forEach((handler) => handler(true))
          resolve()
        }

        this.ws.onmessage = (event) => {
          try {
            const message: IRCMessage = JSON.parse(event.data)
            this.messageHandlers.forEach((handler) => handler(message))
          } catch (error) {
            console.error("[IRC] Failed to parse message:", error)
          }
        }

        this.ws.onclose = () => {
          console.log("[IRC] WebSocket disconnected")
          this.connectionHandlers.forEach((handler) => handler(false))
          this.attemptReconnect()
        }

        this.ws.onerror = (error) => {
          console.error("[IRC] WebSocket error:", error)
          reject(error)
        }
      } catch (error) {
        reject(error)
      }
    })
  }

  private attemptReconnect() {
    if (this.reconnectAttempts < this.maxReconnectAttempts) {
      this.reconnectAttempts++
      console.log(`[IRC] Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})`)

      setTimeout(() => {
        this.connect().catch(console.error)
      }, this.reconnectDelay * this.reconnectAttempts)
    }
  }

  disconnect() {
    if (this.ws) {
      this.ws.close()
      this.ws = null
    }
  }

  send(message: any) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(message))
    } else {
      console.warn("[IRC] WebSocket not connected, cannot send message")
    }
  }

  onMessage(handler: (message: IRCMessage) => void) {
    this.messageHandlers.push(handler)
  }

  onConnection(handler: (connected: boolean) => void) {
    this.connectionHandlers.push(handler)
  }

  isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN
  }
}
