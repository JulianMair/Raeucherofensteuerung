// Fake-WebSocket für die Tests (WEB-05): Der Test entscheidet, wann die
// Verbindung steht, welche Nachricht kommt und wann sie abbricht.
import type { SocketFactory, SocketLike } from "../src/api/live";

export class FakeSocket implements SocketLike {
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: unknown }) => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  closed = false;

  constructor(readonly url: string) {}

  open(): void {
    this.onopen?.();
  }

  /* Objekte gehen wie vom Gerät als JSON-Text hinaus */
  send(data: unknown): void {
    this.onmessage?.({ data: typeof data === "string" ? data : JSON.stringify(data) });
  }

  /* Abbruch von der Gegenseite */
  drop(): void {
    this.onclose?.();
  }

  close(): void {
    this.closed = true;
    this.onclose?.();
  }
}

export function fakeSockets() {
  const sockets: FakeSocket[] = [];
  const factory: SocketFactory = (url) => {
    const socket = new FakeSocket(url);
    sockets.push(socket);
    return socket;
  };
  const latest = (): FakeSocket => {
    const socket = sockets.at(-1);
    if (socket === undefined) {
      throw new Error("noch kein WebSocket angelegt");
    }
    return socket;
  };
  return { factory, sockets, latest };
}
