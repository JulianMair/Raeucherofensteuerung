// REST-Aufrufe nach docs/api.yaml (API-02, E-043). Keine Funktion wirft:
// Jede liefert ein Ergebnis, damit die Oberfläche jeden Fall anzeigen kann.
import type {
  CommandResult,
  Limits,
  OffsetRequest,
  RequestError,
  Role,
  RoleRequest,
  SetpointRequest,
} from "./types";

export type CommandOutcome =
  /* Das Gerät hat geantwortet: angenommen oder mit Grund abgelehnt */
  | { kind: "done"; status: number; seq: number | undefined; result: CommandResult }
  /* Die Anfrage kam nicht bis zum Regeltask: kaputt, zu groß, busy, timeout, not_ready */
  | { kind: "error"; status: number; error: RequestError }
  /* Keine Verbindung oder eine Antwort, die nicht zur API passt */
  | { kind: "network" };

// Absolute Adresse: Im Browser die eigene Seite, in den Tests unter Node
// die Adresse von jsdom. Node löst relative Adressen nicht auf.
function url(path: string): string {
  return new URL(path, window.location.href).toString();
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

// Nur die Form wird geprüft. Ob ein Wert gültig ist, weiß das Gerät.
function toOutcome(status: number, body: unknown): CommandOutcome {
  if (isObject(body) && typeof body.result === "string") {
    const seq = typeof body.seq === "number" ? body.seq : undefined;
    return { kind: "done", status, seq, result: body.result as CommandResult };
  }
  if (isObject(body) && typeof body.error === "string") {
    return { kind: "error", status, error: body as RequestError };
  }
  return { kind: "network" };
}

async function send(method: "POST" | "PUT", path: string, body?: object): Promise<CommandOutcome> {
  try {
    const init: RequestInit = { method };
    if (body !== undefined) {
      init.headers = { "Content-Type": "application/json" };
      init.body = JSON.stringify(body);
    }
    const response = await fetch(url(path), init);
    return toOutcome(response.status, await response.json());
  } catch {
    return { kind: "network" };
  }
}

export function setSetpoint(setpointC: number): Promise<CommandOutcome> {
  const body: SetpointRequest = { setpoint_c: setpointC };
  return send("POST", "/api/setpoint", body);
}

export function start(): Promise<CommandOutcome> {
  return send("POST", "/api/start");
}

export function stop(): Promise<CommandOutcome> {
  return send("POST", "/api/stop");
}

export function reset(): Promise<CommandOutcome> {
  return send("POST", "/api/reset");
}

export function setRole(sensorId: string, role: Role): Promise<CommandOutcome> {
  const body: RoleRequest = { role };
  return send("PUT", `/api/sensors/${encodeURIComponent(sensorId)}/role`, body);
}

export function setOffset(sensorId: string, offsetC: number): Promise<CommandOutcome> {
  const body: OffsetRequest = { offset_c: offsetC };
  return send("PUT", `/api/sensors/${encodeURIComponent(sensorId)}/offset`, body);
}

/* Grenzen für die Prüfung der Eingaben. null, wenn sie (noch) nicht zu haben sind. */
export async function getLimits(): Promise<Limits | null> {
  try {
    const response = await fetch(url("/api/config"));
    if (!response.ok) {
      return null;
    }
    const body: unknown = await response.json();
    return isObject(body) && typeof body.setpoint_min_c === "number" ? (body as Limits) : null;
  } catch {
    return null;
  }
}
