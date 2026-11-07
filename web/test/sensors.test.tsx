import { fireEvent, render, screen, waitFor } from "@testing-library/preact";
import { http, HttpResponse } from "msw";
import { describe, expect, it } from "vitest";

import type { Channel } from "../src/api/types";
import { SensorList } from "../src/components/SensorList";
import { exampleLimits, exampleState } from "./fixtures";
import { requests, server } from "./server";

const CHAMBER = "28AA000000000001";

function renderSensors(channels: Channel[] = exampleState.channels, locked = false, overflow = false) {
  return render(
    <SensorList channels={channels} overflow={overflow} limits={exampleLimits} locked={locked} disabled={false} />,
  );
}

async function messageText(): Promise<string> {
  await waitFor(() => expect(screen.getByTestId("message").textContent).not.toBe(""));
  return screen.getByTestId("message").textContent ?? "";
}

describe("Sensoren", () => {
  it("zeigt jeden Kanal mit ID, Wert, Zustand und Rolle", () => {
    renderSensors();

    expect(screen.getAllByRole("listitem")).toHaveLength(3);
    expect(screen.getByText(CHAMBER)).toBeDefined();
    expect(screen.getByLabelText<HTMLSelectElement>(`Rolle von ${CHAMBER}`).value).toBe("chamber");
    expect(screen.getByText("79,9 °C")).toBeDefined();
    expect(screen.getByText("nicht gefunden")).toBeDefined();
    expect(screen.getByLabelText<HTMLInputElement>("Offset von 28BB000000000002 (°C)").value).toBe("-0,5");
  });

  it("markiert doppelte IDs", () => {
    const first = exampleState.channels[0];
    if (first === undefined) {
      throw new Error("Beispiel ohne Kanal");
    }
    renderSensors([{ ...first, duplicate: true, temp_c: null }]);
    expect(screen.getByText("doppelt")).toBeDefined();
  });

  it("weist eine Rolle zu und zeigt das Ergebnis", async () => {
    server.use(http.put("/api/sensors/:id/role", () => HttpResponse.json({ seq: 9, result: "ok" })));
    renderSensors();

    fireEvent.change(screen.getByLabelText(`Rolle von ${CHAMBER}`), { target: { value: "probe_2" } });

    expect(await messageText()).toBe("Ausgeführt");
    expect(requests).toEqual([{ method: "PUT", path: `/api/sensors/${CHAMBER}/role`, body: { role: "probe_2" } }]);
  });

  it("setzt einen gültigen Offset und lehnt einen ungültigen ab, ohne zu senden", async () => {
    server.use(http.put("/api/sensors/:id/offset", () => HttpResponse.json({ seq: 10, result: "ok" })));
    renderSensors();
    const input = screen.getByLabelText(`Offset von ${CHAMBER} (°C)`);
    const submit = screen.getAllByRole("button", { name: "Übernehmen" })[0];
    if (submit === undefined) {
      throw new Error("kein Knopf");
    }

    fireEvent.input(input, { target: { value: "6" } });
    fireEvent.click(submit);
    expect(await messageText()).toBe("Höchstens ±5 °C");
    expect(requests).toEqual([]);

    fireEvent.input(input, { target: { value: "0,5" } });
    fireEvent.click(submit);
    await waitFor(() => expect(screen.getByTestId("message").textContent).toBe("Ausgeführt"));
    expect(requests).toEqual([{ method: "PUT", path: `/api/sensors/${CHAMBER}/offset`, body: { offset_c: 0.5 } }]);
  });

  /* E-022: beim Heizen gesperrt, und das Gerät lehnt es ebenfalls ab */
  it("sperrt Rollen und Offsets beim Heizen", () => {
    renderSensors(exampleState.channels, true);

    expect(screen.getByText("Rollen und Offsets sind beim Heizen gesperrt.")).toBeDefined();
    expect(screen.getByLabelText<HTMLSelectElement>(`Rolle von ${CHAMBER}`).disabled).toBe(true);
    expect(screen.getByLabelText<HTMLInputElement>(`Offset von ${CHAMBER} (°C)`).disabled).toBe(true);
  });

  it("zeigt die Ablehnung durch das Gerät", async () => {
    server.use(
      http.put("/api/sensors/:id/role", () => HttpResponse.json({ seq: 11, result: "sensors_locked" }, { status: 409 })),
    );
    renderSensors();
    fireEvent.change(screen.getByLabelText(`Rolle von ${CHAMBER}`), { target: { value: "unassigned" } });

    expect(await messageText()).toBe("Beim Heizen gesperrt");
  });

  it("meldet zu viele Sensoren und eine leere Liste", () => {
    const many = renderSensors(exampleState.channels, false, true);
    expect(screen.getByText(/Mehr Sensoren am Bus/)).toBeDefined();
    many.unmount();

    renderSensors([]);
    expect(screen.getByText("Keine Sensoren gefunden.")).toBeDefined();
  });
});
