// Livewerte über den WebSocket /ws (API-03, WEB-03, E-043).
//
// Das Gerät schickt jede Sekunde den Zustand. Bricht die Verbindung ab,
// verbindet sich die Oberfläche selbst neu, mit wachsender Wartezeit.
// Kommt zu lange keine Nachricht, gilt die Verbindung ebenfalls als
// getrennt: Verlässt ein Handy das WLAN, meldet TCP das oft erst nach Minuten.
import type { StateSnapshot } from "./types";

/* Was die Oberfläche von einem WebSocket braucht. Die Tests setzen einen Fake ein. */
export interface SocketLike {
  onopen: (() => void) | null;
  onmessage: ((event: { data: unknown }) => void) | null;
  onclose: (() => void) | null;
  onerror: (() => void) | null;
  close(): void;
}

export type SocketFactory = (url: string) => SocketLike;

export type LiveStatus = "connecting" | "open" | "closed";

export interface LiveCallbacks {
  onState: (state: StateSnapshot) => void;
  onStatus: (status: LiveStatus) => void;
}

export const RECONNECT_DELAYS_MS = [1000, 2000, 4000, 5000];
/* Drei verpasste Nachrichten und etwas Luft */
export const SILENCE_LIMIT_MS = 3500;

/* Der echte WebSocket des Browsers hinter derselben schmalen Schnittstelle */
export const browserSocket: SocketFactory = (url) => {
  const ws = new WebSocket(url);
  const socket: SocketLike = { onopen: null, onmessage: null, onclose: null, onerror: null, close: () => ws.close() };
  ws.onopen = () => socket.onopen?.();
  ws.onmessage = (event: MessageEvent) => socket.onmessage?.({ data: event.data });
  ws.onclose = () => socket.onclose?.();
  ws.onerror = () => socket.onerror?.();
  return socket;
};

export function wsUrl(location: { protocol: string; host: string }): string {
  return `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws`;
}

function parseState(data: unknown): StateSnapshot | null {
  if (typeof data !== "string") {
    return null;
  }
  try {
    const value: unknown = JSON.parse(data);
    const ok = typeof value === "object" && value !== null && !Array.isArray(value) && "state" in value;
    return ok ? (value as StateSnapshot) : null;
  } catch {
    return null;
  }
}

/* Startet die Verbindung. Liefert eine Funktion, die sie dauerhaft beendet. */
export function connectLive(createSocket: SocketFactory, url: string, callbacks: LiveCallbacks): () => void {
  let socket: SocketLike | null = null;
  let attempt = 0;
  let stopped = false;
  let reconnectTimer: ReturnType<typeof setTimeout> | undefined;
  let silenceTimer: ReturnType<typeof setTimeout> | undefined;

  const watchSilence = () => {
    clearTimeout(silenceTimer);
    silenceTimer = setTimeout(() => socket?.close(), SILENCE_LIMIT_MS);
  };

  const scheduleReconnect = () => {
    clearTimeout(silenceTimer);
    const delay = RECONNECT_DELAYS_MS[Math.min(attempt, RECONNECT_DELAYS_MS.length - 1)];
    attempt++;
    reconnectTimer = setTimeout(open, delay);
  };

  function open() {
    callbacks.onStatus("connecting");
    const current = createSocket(url);
    socket = current;
    current.onopen = () => {
      attempt = 0;
      callbacks.onStatus("open");
      watchSilence();
    };
    current.onmessage = (event) => {
      const state = parseState(event.data);
      if (state !== null) {
        watchSilence();
        callbacks.onState(state);
      }
    };
    current.onclose = () => {
      // Ein alter WebSocket, der sich spät meldet, zählt nicht mehr
      if (stopped || socket !== current) {
        return;
      }
      socket = null;
      callbacks.onStatus("closed");
      scheduleReconnect();
    };
    current.onerror = () => {
      // Darauf folgt immer onclose
    };
  }

  open();
  return () => {
    stopped = true;
    clearTimeout(reconnectTimer);
    clearTimeout(silenceTimer);
    socket?.close();
  };
}
