export interface IRCMessage {
  type: "chat" | "join" | "part" | "nick_change" | "server_status"
  server: string
  channel?: string
  nick?: string
  text?: string
  timestamp: number
  level: number // MSGLEVEL_*
}

export interface Nick {
  nick: string
  mode: "@" | "+" | "" // op, voice, regular
  away?: boolean
  realname?: string
}

export interface Channel {
  name: string
  server: string
  topic?: string
  nicks: Nick[]
  messages: IRCMessage[]
  unread: number
  highlight: boolean
}

export interface Server {
  tag: string
  address: string
  port: number
  connected: boolean
  nick: string
  channels: Channel[]
}

// MSGLEVEL constants (from irssi)
export const MSGLEVEL = {
  CRAP: 0x0001,
  MSGS: 0x0002,
  PUBLIC: 0x0004,
  NOTICES: 0x0008,
  SNOTES: 0x0010,
  CTCPS: 0x0020,
  ACTIONS: 0x0040,
  JOINS: 0x0080,
  PARTS: 0x0100,
  QUITS: 0x0200,
  KICKS: 0x0400,
  MODES: 0x0800,
  TOPICS: 0x1000,
  WALLOPS: 0x2000,
  INVITES: 0x4000,
  NICKS: 0x8000,
  DCC: 0x10000,
  DCCMSGS: 0x20000,
  CLIENTNOTICE: 0x40000,
  CLIENTCRAP: 0x80000,
  CLIENTERROR: 0x100000,
  HILIGHT: 0x200000,
} as const

export type MSGLevel = (typeof MSGLEVEL)[keyof typeof MSGLEVEL]
