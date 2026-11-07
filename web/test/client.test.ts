import { http, HttpResponse } from "msw";
import { describe, expect, it } from "vitest";

import { getLimits, reset, setCoreTarget, setOffset, setRole, setSetpoint, start } from "../src/api/client";
import { exampleLimits } from "./fixtures";
import { requests, server } from "./server";

/* Wartet, bis MSW die gesendete Anfrage aufgezeichnet hat */
async function sent() {
  await new Promise((resolve) => setTimeout(resolve, 0));
  return requests;
}

describe("REST-Client", () => {
  it("sendet den Sollwert als JSON und liefert das Ergebnis mit Nummer", async () => {
    server.use(http.post("/api/setpoint", () => HttpResponse.json({ seq: 12, result: "ok" })));

    const outcome = await setSetpoint(80);

    expect(outcome).toEqual({ kind: "done", status: 200, seq: 12, result: "ok" });
    expect(await sent()).toEqual([{ method: "POST", path: "/api/setpoint", body: { setpoint_c: 80 } }]);
  });

  it("liefert den Grund, wenn proto den Sollwert ablehnt (422, ohne Nummer)", async () => {
    server.use(
      http.post("/api/setpoint", () => HttpResponse.json({ result: "setpoint_too_high" }, { status: 422 })),
    );

    expect(await setSetpoint(500)).toEqual({ kind: "done", status: 422, seq: undefined, result: "setpoint_too_high" });
  });

  it("liefert den Grund, wenn der Regeltask ablehnt (409)", async () => {
    server.use(http.post("/api/start", () => HttpResponse.json({ seq: 3, result: "no_setpoint" }, { status: 409 })));

    expect(await start()).toEqual({ kind: "done", status: 409, seq: 3, result: "no_setpoint" });
  });

  it("start, stop und reset senden keinen Inhalt", async () => {
    server.use(http.post("/api/reset", () => HttpResponse.json({ seq: 4, result: "not_in_safe_state" }, { status: 409 })));

    await reset();

    expect(await sent()).toEqual([{ method: "POST", path: "/api/reset", body: undefined }]);
  });

  it("meldet busy, timeout und not_ready als Fehler des Webservers", async () => {
    server.use(
      http.post("/api/start", () => HttpResponse.json({ error: "busy" }, { status: 503 }), { once: true }),
      http.post("/api/start", () => HttpResponse.json({ error: "timeout", seq: 7 }, { status: 504 }), { once: true }),
    );

    expect(await start()).toEqual({ kind: "error", status: 503, error: { error: "busy" } });
    expect(await start()).toEqual({ kind: "error", status: 504, error: { error: "timeout", seq: 7 } });
  });

  it("meldet Netzfehler und Antworten, die nicht zur API passen", async () => {
    server.use(
      http.post("/api/start", () => HttpResponse.error(), { once: true }),
      http.post("/api/start", () => new HttpResponse("kein JSON"), { once: true }),
      http.post("/api/start", () => HttpResponse.json({ foo: 1 }), { once: true }),
    );

    expect(await start()).toEqual({ kind: "network" });
    expect(await start()).toEqual({ kind: "network" });
    expect(await start()).toEqual({ kind: "network" });
  });

  it("schickt Rolle und Offset an den Pfad des Sensors", async () => {
    server.use(
      http.put("/api/sensors/:id/role", () => HttpResponse.json({ seq: 1, result: "ok" })),
      http.put("/api/sensors/:id/offset", () => HttpResponse.json({ seq: 2, result: "ok" })),
    );

    await setRole("28AA000000000001", "chamber");
    await setOffset("28AA000000000001", -0.5);

    expect(await sent()).toEqual([
      { method: "PUT", path: "/api/sensors/28AA000000000001/role", body: { role: "chamber" } },
      { method: "PUT", path: "/api/sensors/28AA000000000001/offset", body: { offset_c: -0.5 } },
    ]);
  });

  it("schickt das Kernziel als JSON", async () => {
    server.use(http.post("/api/core-target", () => HttpResponse.json({ seq: 5, result: "ok" })));

    expect(await setCoreTarget(68)).toEqual({ kind: "done", status: 200, seq: 5, result: "ok" });
    expect(await sent()).toEqual([{ method: "POST", path: "/api/core-target", body: { core_target_c: 68 } }]);
  });

  it("liest die Grenzen, null wenn das Gerät noch nicht bereit ist", async () => {
    expect(await getLimits()).toEqual(exampleLimits);

    server.use(http.get("/api/config", () => HttpResponse.json({ error: "not_ready" }, { status: 503 })));
    expect(await getLimits()).toBeNull();
  });
});
