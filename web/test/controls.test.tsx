import { fireEvent, render, screen, waitFor } from "@testing-library/preact";
import { http, HttpResponse } from "msw";
import { describe, expect, it } from "vitest";

import type { State } from "../src/api/types";
import { Controls } from "../src/components/Controls";
import { exampleLimits } from "./fixtures";
import { requests, server } from "./server";

function renderControls(state: State = "idle", disabled = false) {
  return render(<Controls state={state} limits={exampleLimits} disabled={disabled} />);
}

function enterSetpoint(text: string) {
  fireEvent.input(screen.getByLabelText("Sollwert (°C)"), { target: { value: text } });
  fireEvent.click(screen.getByRole("button", { name: "Setzen" }));
}

async function messageText(): Promise<string> {
  await waitFor(() => expect(screen.getByTestId("message").textContent).not.toBe(""));
  return screen.getByTestId("message").textContent ?? "";
}

const button = (name: string) => screen.getByRole<HTMLButtonElement>("button", { name });

describe("Bedienung", () => {
  /* WEB-06: ungültige Eingaben gehen nicht hinaus */
  it.each(["", "abc", "19", "111"])("schickt den ungültigen Sollwert %j nicht", async (text) => {
    renderControls();
    enterSetpoint(text);

    expect(await messageText()).not.toBe("Ausgeführt");
    await new Promise((resolve) => setTimeout(resolve, 10));
    expect(requests).toEqual([]);
  });

  it("schickt einen gültigen Sollwert mit Komma als Zahl", async () => {
    server.use(http.post("/api/setpoint", () => HttpResponse.json({ seq: 1, result: "ok" })));
    renderControls();
    enterSetpoint("80,5");

    expect(await messageText()).toBe("Ausgeführt");
    expect(requests).toEqual([{ method: "POST", path: "/api/setpoint", body: { setpoint_c: 80.5 } }]);
  });

  /* WEB-06: abgelehnter Sollwert, der Grund kommt vom Gerät */
  it("zeigt den Grund, wenn das Gerät den Sollwert ablehnt", async () => {
    server.use(
      http.post("/api/setpoint", () => HttpResponse.json({ result: "setpoint_too_high" }, { status: 422 })),
    );
    renderControls();
    enterSetpoint("100");

    expect(await messageText()).toBe("Sollwert zu hoch");
  });

  it("startet in BEREIT und zeigt den Grund einer Ablehnung", async () => {
    server.use(http.post("/api/start", () => HttpResponse.json({ seq: 2, result: "no_setpoint" }, { status: 409 })));
    renderControls("idle");
    fireEvent.click(button("Start"));

    expect(await messageText()).toBe("Erst einen Sollwert setzen");
  });

  it("gibt Stopp nur beim Heizen frei", () => {
    const idle = renderControls("idle");
    expect(button("Stopp").disabled).toBe(true);
    idle.unmount();

    renderControls("holding");
    expect(button("Stopp").disabled).toBe(false);
    expect(button("Start").disabled).toBe(true);
  });

  /* WEB-06: sicherer Zustand */
  it("sperrt im sicheren Zustand den Start und gibt nur dann den Reset frei", async () => {
    server.use(http.post("/api/reset", () => HttpResponse.json({ seq: 5, result: "cause_active" }, { status: 409 })));
    renderControls("safe_state");

    expect(button("Start").disabled).toBe(true);
    expect(button("Sicheren Zustand zurücksetzen").disabled).toBe(false);
    fireEvent.click(button("Sicheren Zustand zurücksetzen"));
    expect(await messageText()).toBe("Die Ursache besteht noch");
  });

  it.each(["idle", "heating", "fault"] as State[])("gibt den Reset in %s nicht frei", (state) => {
    renderControls(state);
    expect(button("Sicheren Zustand zurücksetzen").disabled).toBe(true);
  });

  it("meldet ein beschäftigtes Gerät und fehlende Verbindung", async () => {
    server.use(
      http.post("/api/start", () => HttpResponse.json({ error: "busy" }, { status: 503 }), { once: true }),
      http.post("/api/start", () => HttpResponse.error()),
    );
    renderControls("idle");

    fireEvent.click(button("Start"));
    expect(await messageText()).toBe("Gerät beschäftigt, bitte erneut versuchen");
    fireEvent.click(button("Start"));
    await waitFor(() => expect(screen.getByTestId("message").textContent).toBe("Keine Verbindung zum Gerät"));
  });

  /* WEB-03: ohne Verbindung keine Befehle */
  it("sperrt alles ohne Verbindung", () => {
    renderControls("safe_state", true);
    for (const name of ["Setzen", "Start", "Stopp", "Sicheren Zustand zurücksetzen"]) {
      expect(button(name).disabled).toBe(true);
    }
    expect(screen.getByLabelText<HTMLInputElement>("Sollwert (°C)").disabled).toBe(true);
  });
});
