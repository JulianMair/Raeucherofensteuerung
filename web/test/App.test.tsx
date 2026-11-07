import { act, render, screen } from "@testing-library/preact";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { RECONNECT_DELAYS_MS } from "../src/api/live";
import { App } from "../src/App";
import { fakeSockets } from "./fakeWebSocket";
import { exampleState, stateWith } from "./fixtures";

let fake: ReturnType<typeof fakeSockets>;

beforeEach(() => {
  vi.useFakeTimers({ toFake: ["setTimeout", "clearTimeout", "setInterval", "clearInterval", "Date"] });
  fake = fakeSockets();
});

afterEach(() => {
  vi.useRealTimers();
});

describe("App mit Liveverbindung", () => {
  it("meldet den Verbindungsaufbau, bis der erste Zustand kommt", async () => {
    render(<App createSocket={fake.factory} />);
    expect(screen.getByRole("status").textContent).toContain("Verbinde");

    await act(() => {
      fake.latest().open();
      fake.latest().send(exampleState);
    });

    expect(screen.queryByRole("status")).toBeNull();
    expect(screen.getByTestId("state").textContent).toBe("Halten");
  });

  /* WEB-06: Verbindungsabbruch und Wiederverbindung */
  it("zeigt den Abbruch mit dem Alter der Daten und erholt sich nach der Wiederverbindung", async () => {
    render(<App createSocket={fake.factory} />);
    await act(() => {
      fake.latest().open();
      fake.latest().send(exampleState);
    });

    await act(() => {
      fake.latest().drop();
    });
    await act(() => {
      vi.advanceTimersByTime(800); /* noch vor dem neuen Versuch */
    });
    expect(screen.getByRole("status").textContent).toContain("Verbindung getrennt");
    expect(screen.getByLabelText("Status").className).toContain("stale");

    await act(() => {
      vi.advanceTimersByTime(3000);
    });
    expect(screen.getByRole("status").textContent).toMatch(/vor [34] s/);

    await act(() => {
      vi.advanceTimersByTime(RECONNECT_DELAYS_MS[0] ?? 0);
      fake.latest().open();
      fake.latest().send(stateWith({ state: "cooling" }));
    });
    expect(screen.queryByRole("status")).toBeNull();
    expect(screen.getByTestId("state").textContent).toBe("Abkühlen");
    expect(screen.getByLabelText("Status").className).not.toContain("stale");
  });
});
