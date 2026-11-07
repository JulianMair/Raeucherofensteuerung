import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { connectLive, type LiveStatus, RECONNECT_DELAYS_MS, SILENCE_LIMIT_MS, wsUrl } from "../src/api/live";
import type { StateSnapshot } from "../src/api/types";
import { fakeSockets } from "./fakeWebSocket";
import { exampleState } from "./fixtures";

let fake: ReturnType<typeof fakeSockets>;
let states: StateSnapshot[];
let statuses: LiveStatus[];
let stop: () => void;

beforeEach(() => {
  vi.useFakeTimers();
  fake = fakeSockets();
  states = [];
  statuses = [];
  stop = connectLive(fake.factory, "ws://192.168.4.1/ws", {
    onState: (state) => states.push(state),
    onStatus: (status) => statuses.push(status),
  });
});

afterEach(() => {
  stop();
  vi.useRealTimers();
});

describe("Liveverbindung", () => {
  it("baut die Adresse aus der eigenen Seite", () => {
    expect(wsUrl(new URL("http://192.168.4.1/"))).toBe("ws://192.168.4.1/ws");
    expect(wsUrl(new URL("https://localhost:5173/x"))).toBe("wss://localhost:5173/ws");
  });

  it("verbindet sich und reicht jeden Zustand weiter", () => {
    expect(fake.latest().url).toBe("ws://192.168.4.1/ws");
    expect(statuses).toEqual(["connecting"]);

    fake.latest().open();
    fake.latest().send(exampleState);

    expect(statuses).toEqual(["connecting", "open"]);
    expect(states).toEqual([exampleState]);
  });

  it("ignoriert Nachrichten, die kein Zustand sind", () => {
    fake.latest().open();
    fake.latest().send("kein JSON");
    fake.latest().send({ foo: 1 });
    fake.latest().send("[1,2]");

    expect(states).toEqual([]);
  });

  /* WEB-03: Abbruch und Wiederverbindung */
  it("meldet einen Abbruch und verbindet sich mit wachsender Wartezeit neu", () => {
    fake.latest().open();
    fake.latest().drop();
    expect(statuses.at(-1)).toBe("closed");

    for (const delay of [...RECONNECT_DELAYS_MS, RECONNECT_DELAYS_MS.at(-1) ?? 0]) {
      const before = fake.sockets.length;
      vi.advanceTimersByTime(delay - 1);
      expect(fake.sockets.length).toBe(before);
      vi.advanceTimersByTime(1);
      expect(fake.sockets.length).toBe(before + 1);
      fake.latest().drop();
    }
  });

  it("fängt nach einer gelungenen Verbindung wieder mit der kürzesten Wartezeit an", () => {
    fake.latest().drop();
    vi.advanceTimersByTime(RECONNECT_DELAYS_MS[0] ?? 0);
    fake.latest().drop();
    vi.advanceTimersByTime(RECONNECT_DELAYS_MS[1] ?? 0);
    fake.latest().open();
    fake.latest().drop();

    const before = fake.sockets.length;
    vi.advanceTimersByTime(RECONNECT_DELAYS_MS[0] ?? 0);
    expect(fake.sockets.length).toBe(before + 1);
  });

  /* Verlässt ein Handy das WLAN, meldet TCP das oft erst nach Minuten */
  it("gilt als getrennt, wenn zu lange keine Nachricht kommt", () => {
    fake.latest().open();
    fake.latest().send(exampleState);
    const first = fake.latest();

    vi.advanceTimersByTime(SILENCE_LIMIT_MS - 1);
    expect(first.closed).toBe(false);
    vi.advanceTimersByTime(1);

    expect(first.closed).toBe(true);
    expect(statuses.at(-1)).toBe("closed");
    vi.advanceTimersByTime(RECONNECT_DELAYS_MS[0] ?? 0);
    expect(fake.latest()).not.toBe(first);
  });

  it("jede Nachricht verlängert die Frist", () => {
    fake.latest().open();
    for (let i = 0; i < 10; i++) {
      vi.advanceTimersByTime(1000);
      fake.latest().send(exampleState);
    }
    expect(fake.latest().closed).toBe(false);
    expect(fake.sockets.length).toBe(1);
  });

  it("hört nach stop auf und verbindet sich nicht neu", () => {
    fake.latest().open();
    const socket = fake.latest();
    stop();

    expect(socket.closed).toBe(true);
    vi.advanceTimersByTime(60000);
    expect(fake.sockets.length).toBe(1);
  });
});
