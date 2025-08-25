"use client"

import type React from "react"
import { useState, useRef, useEffect, useCallback } from "react"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Dialog, DialogContent, DialogHeader, DialogTitle } from "@/components/ui/dialog"
import { Label } from "@/components/ui/label"
import { IRCWebSocketClient } from "@/lib/websocket-client"
import type { IRCMessage, Server } from "@/lib/types"
import { MSGLEVEL } from "@/lib/types"

export default function IRCClient() {
  const [servers, setServers] = useState<Server[]>([])
  const [activeServer, setActiveServer] = useState<string>("")
  const [activeChannel, setActiveChannel] = useState<string>("")
  const [inputValue, setInputValue] = useState("")
  const [messageHistory, setMessageHistory] = useState<string[]>([])
  const [historyIndex, setHistoryIndex] = useState(-1)
  const [showConnectionDialog, setShowConnectionDialog] = useState(false)
  const [connected, setConnected] = useState(false)
  const [connectionForm, setConnectionForm] = useState({
    tag: "",
    address: "",
    port: 6667,
    nick: "",
    autoConnect: true,
  })

  const inputRef = useRef<HTMLInputElement>(null)
  const [ircClient] = useState(() => new IRCWebSocketClient())

  useEffect(() => {
    const initConnection = async () => {
      try {
        await ircClient.connect()
      } catch (error) {
        console.error("[IRC] Failed to connect:", error)
      }
    }

    ircClient.onConnection((isConnected) => {
      setConnected(isConnected)
    })

    ircClient.onMessage((message: IRCMessage) => {
      handleIncomingMessage(message)
    })

    initConnection()

    return () => {
      ircClient.disconnect()
    }
  }, [])

  const handleIncomingMessage = useCallback(
    (message: IRCMessage) => {
      setServers((prevServers) => {
        const serverIndex = prevServers.findIndex((s) => s.tag === message.server)
        if (serverIndex === -1) return prevServers

        const updatedServers = [...prevServers]
        const server = { ...updatedServers[serverIndex] }

        if (message.channel) {
          const channelIndex = server.channels.findIndex((c) => c.name === message.channel)
          if (channelIndex !== -1) {
            const channel = { ...server.channels[channelIndex] }
            channel.messages = [...channel.messages, message]

            // Update unread count if not active channel
            if (!(activeServer === message.server && activeChannel === message.channel)) {
              channel.unread++
              if (message.level & MSGLEVEL.HILIGHT) {
                channel.highlight = true
              }
            }

            server.channels[channelIndex] = channel
          }
        }

        updatedServers[serverIndex] = server
        return updatedServers
      })
    },
    [activeServer, activeChannel],
  )

  const sendCommand = (command: string) => {
    if (!connected) {
      console.warn("[IRC] Not connected to WebSocket server")
      return
    }

    ircClient.send({
      type: "command",
      server: activeServer,
      channel: activeChannel,
      command: command,
    })
  }

  const handleSendMessage = () => {
    if (!inputValue.trim()) return

    setMessageHistory((prev) => [...prev.slice(-49), inputValue])
    setHistoryIndex(-1)

    if (inputValue.startsWith("/")) {
      sendCommand(inputValue)
    } else {
      sendCommand(`/msg ${activeChannel} ${inputValue}`)
    }

    setInputValue("")
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === "Enter") {
      handleSendMessage()
    } else if (e.key === "Tab") {
      e.preventDefault()
      // Tab completion logic here
    } else if (e.key === "ArrowUp") {
      e.preventDefault()
      if (historyIndex < messageHistory.length - 1) {
        const newIndex = historyIndex + 1
        setHistoryIndex(newIndex)
        setInputValue(messageHistory[messageHistory.length - 1 - newIndex])
      }
    } else if (e.key === "ArrowDown") {
      e.preventDefault()
      if (historyIndex > 0) {
        const newIndex = historyIndex - 1
        setHistoryIndex(newIndex)
        setInputValue(messageHistory[messageHistory.length - 1 - newIndex])
      } else if (historyIndex === 0) {
        setHistoryIndex(-1)
        setInputValue("")
      }
    }
  }

  const addServer = () => {
    if (!connectionForm.tag || !connectionForm.address) return

    const newServer: Server = {
      tag: connectionForm.tag,
      address: connectionForm.address,
      port: connectionForm.port,
      connected: false,
      nick: connectionForm.nick,
      channels: [],
    }

    setServers((prev) => [...prev, newServer])
    setShowConnectionDialog(false)
    setConnectionForm({ tag: "", address: "", port: 6667, nick: "", autoConnect: true })

    if (connectionForm.autoConnect) {
      sendCommand(`/connect ${connectionForm.tag}`)
    }
  }

  const getCurrentServer = () => servers.find((s) => s.tag === activeServer)
  const getCurrentChannel = () => getCurrentServer()?.channels.find((c) => c.name === activeChannel)

  const renderMessage = (message: IRCMessage) => {
    const timestamp = new Date(message.timestamp).toLocaleTimeString("en-US", {
      hour12: false,
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    })

    const baseClasses = "flex text-xs leading-tight font-mono"

    switch (message.type) {
      case "chat":
        return (
          <div
            key={`${message.timestamp}-${message.nick}`}
            className={`${baseClasses} ${message.level & MSGLEVEL.HILIGHT ? "bg-yellow-900/30" : ""}`}
          >
            <span className="text-gray-600 w-16 flex-shrink-0 text-right mr-2 select-none">{timestamp}</span>
            <span className="text-cyan-400 w-20 flex-shrink-0 font-bold text-right mr-2">{message.nick}</span>
            <span className="text-gray-300 flex-1">{message.text}</span>
          </div>
        )
      case "join":
        return (
          <div key={`${message.timestamp}-join`} className={`${baseClasses} text-green-400`}>
            <span className="text-gray-600 w-16 flex-shrink-0 text-right mr-2 select-none">{timestamp}</span>
            <span className="flex-1">
              → {message.nick} has joined {message.channel}
            </span>
          </div>
        )
      case "part":
        return (
          <div key={`${message.timestamp}-part`} className={`${baseClasses} text-red-400`}>
            <span className="text-gray-600 w-16 flex-shrink-0 text-right mr-2 select-none">{timestamp}</span>
            <span className="flex-1">
              ← {message.nick} has left {message.channel}
            </span>
          </div>
        )
      case "server_status":
        return (
          <div key={`${message.timestamp}-status`} className={`${baseClasses} text-yellow-400`}>
            <span className="text-gray-600 w-16 flex-shrink-0 text-right mr-2 select-none">{timestamp}</span>
            <span className="flex-1">* {message.text}</span>
          </div>
        )
      default:
        return null
    }
  }

  return (
    <div className="h-screen bg-black text-gray-100 font-mono text-xs flex flex-col overflow-hidden">
      <div className="bg-gray-900 border-b border-gray-700 px-3 py-1 flex items-center justify-between">
        <div className="flex items-center space-x-3">
          <span className="text-cyan-400 font-bold text-sm">irssip</span>
          <span className="text-gray-600">|</span>
          <span className={`text-xs ${connected ? "text-green-400" : "text-red-400"}`}>
            {connected ? "● WebSocket Connected" : "● WebSocket Disconnected"}
          </span>
          <button
            onClick={() => setShowConnectionDialog(true)}
            className="text-cyan-400 hover:text-cyan-300 text-xs px-2 py-1 border border-gray-600 hover:bg-gray-800 transition-colors rounded"
          >
            + Server
          </button>
        </div>
        <div className="text-gray-500 text-xs">{new Date().toLocaleTimeString()}</div>
      </div>

      {showConnectionDialog && (
        <Dialog open={showConnectionDialog} onOpenChange={setShowConnectionDialog}>
          <DialogContent className="bg-gray-800 border border-gray-600 text-gray-100">
            <DialogHeader>
              <DialogTitle className="text-cyan-400">Add IRC Server</DialogTitle>
            </DialogHeader>
            <div className="space-y-4">
              <div>
                <Label className="text-gray-300">Server Tag</Label>
                <Input
                  value={connectionForm.tag}
                  onChange={(e) => setConnectionForm((prev) => ({ ...prev, tag: e.target.value }))}
                  className="bg-gray-700 border-gray-600 text-gray-100"
                  placeholder="freenode"
                />
              </div>
              <div>
                <Label className="text-gray-300">Address</Label>
                <Input
                  value={connectionForm.address}
                  onChange={(e) => setConnectionForm((prev) => ({ ...prev, address: e.target.value }))}
                  className="bg-gray-700 border-gray-600 text-gray-100"
                  placeholder="irc.libera.chat"
                />
              </div>
              <div>
                <Label className="text-gray-300">Port</Label>
                <Input
                  type="number"
                  value={connectionForm.port}
                  onChange={(e) =>
                    setConnectionForm((prev) => ({ ...prev, port: Number.parseInt(e.target.value) || 6667 }))
                  }
                  className="bg-gray-700 border-gray-600 text-gray-100"
                />
              </div>
              <div>
                <Label className="text-gray-300">Nickname</Label>
                <Input
                  value={connectionForm.nick}
                  onChange={(e) => setConnectionForm((prev) => ({ ...prev, nick: e.target.value }))}
                  className="bg-gray-700 border-gray-600 text-gray-100"
                  placeholder="your_nick"
                />
              </div>
              <Button onClick={addServer} className="w-full bg-cyan-600 hover:bg-cyan-700">
                Add Server
              </Button>
            </div>
          </DialogContent>
        </Dialog>
      )}

      <div className="flex flex-1 overflow-hidden">
        <div className="w-64 bg-gray-900 border-r border-gray-700 flex flex-col">
          <div className="px-3 py-2 border-b border-gray-700 bg-gray-800">
            <h3 className="text-white font-bold text-sm">Servers & Channels</h3>
          </div>
          <div className="flex-1 overflow-y-auto p-2">
            {servers.map((server) => (
              <div key={server.tag} className="mb-4">
                <div
                  className={`flex items-center justify-between p-2 rounded cursor-pointer hover:bg-gray-800 ${
                    activeServer === server.tag ? "bg-gray-800" : ""
                  }`}
                  onClick={() => setActiveServer(server.tag)}
                >
                  <div className="flex items-center space-x-2">
                    <span className={`w-2 h-2 rounded-full ${server.connected ? "bg-green-400" : "bg-red-400"}`} />
                    <span className="text-white font-semibold">{server.tag}</span>
                  </div>
                  <span className="text-gray-500 text-xs">
                    {server.address}:{server.port}
                  </span>
                </div>
                <div className="ml-4 mt-1 space-y-1">
                  {server.channels.map((channel) => (
                    <div
                      key={channel.name}
                      className={`flex items-center justify-between p-1 rounded cursor-pointer hover:bg-gray-800 ${
                        activeServer === server.tag && activeChannel === channel.name ? "bg-gray-800" : ""
                      }`}
                      onClick={() => {
                        setActiveServer(server.tag)
                        setActiveChannel(channel.name)
                      }}
                    >
                      <span className={`text-sm ${channel.highlight ? "text-yellow-400" : "text-gray-300"}`}>
                        {channel.name}
                      </span>
                      <div className="flex items-center space-x-1">
                        {channel.unread > 0 && (
                          <span
                            className={`px-1 py-0 text-xs rounded ${
                              channel.highlight ? "bg-yellow-600 text-black" : "bg-blue-600 text-white"
                            }`}
                          >
                            {channel.unread}
                          </span>
                        )}
                        <span className="text-gray-500 text-xs">{channel.nicks.length}</span>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            ))}
          </div>
        </div>

        <div className="flex-1 flex flex-col bg-black">
          <div className="bg-gray-900 border-b border-gray-700 px-3 py-2">
            <div className="flex items-center justify-between">
              <div className="flex items-center space-x-3">
                <span className="text-white font-bold">{activeChannel || "No channel selected"}</span>
                {getCurrentChannel()?.topic && (
                  <span className="text-gray-400 text-sm">Topic: {getCurrentChannel()?.topic}</span>
                )}
              </div>
              <span className="text-gray-500 text-sm">Users: {getCurrentChannel()?.nicks.length || 0}</span>
            </div>
          </div>

          <div className="flex-1 overflow-y-auto px-3 py-2 space-y-1">
            {getCurrentChannel()?.messages.map(renderMessage)}
          </div>

          <div className="border-t border-gray-700 p-3 bg-gray-900">
            <div className="flex items-center space-x-2">
              <span className="text-cyan-400 font-bold text-sm">[{activeChannel || "none"}]</span>
              <Input
                ref={inputRef}
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
                onKeyDown={handleKeyDown}
                className="flex-1 bg-gray-800 border-gray-600 text-gray-100 font-mono"
                placeholder={connected ? "Type message or /command..." : "WebSocket disconnected"}
                disabled={!connected}
              />
              <span className="text-gray-500 text-xs font-mono">{inputValue.startsWith("/") ? "CMD" : "MSG"}</span>
            </div>
          </div>
        </div>

        <div className="w-48 bg-gray-900 border-l border-gray-700 flex flex-col">
          <div className="px-3 py-2 border-b border-gray-700">
            <h3 className="text-white font-bold text-sm">Users ({getCurrentChannel()?.nicks.length || 0})</h3>
          </div>
          <div className="flex-1 overflow-y-auto p-2">
            {getCurrentChannel()?.nicks.map((nick) => (
              <div
                key={nick.nick}
                className={`flex items-center space-x-1 p-1 text-sm hover:bg-gray-800 rounded ${
                  nick.away ? "opacity-60" : ""
                }`}
              >
                <span
                  className={`font-bold ${
                    nick.mode === "@" ? "text-red-400" : nick.mode === "+" ? "text-yellow-400" : "text-gray-500"
                  }`}
                >
                  {nick.mode}
                </span>
                <span className="text-gray-300">{nick.nick}</span>
              </div>
            ))}
          </div>
        </div>
      </div>

      <div className="bg-gray-800 border-t border-gray-700 px-3 py-1 flex items-center justify-between text-xs">
        <div className="flex items-center space-x-4">
          <span className="text-gray-400">Server: {getCurrentServer()?.tag || "none"}</span>
          <span className="text-gray-400">Channel: {activeChannel || "none"}</span>
        </div>
        <div className="text-gray-500">WebSocket: {connected ? "Connected" : "Disconnected"}</div>
      </div>
    </div>
  )
}
