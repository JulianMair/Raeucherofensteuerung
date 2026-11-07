// Deutsche Texte für alle Werte der API (WEB-02). Jede Tabelle ist ein
// Record über den erzeugten Typ: Kommt in docs/api.yaml ein Wert dazu,
// kompiliert die Oberfläche erst wieder, wenn er hier einen Text hat.
import type { CommandResult, Fault, ReadStatus, RequestErrorCode, Role, RoleStatus, State } from "./api/types";

export const STATE_LABELS: Record<State, string> = {
  idle: "Bereit",
  heating: "Aufheizen",
  holding: "Halten",
  cooling: "Abkühlen",
  fault: "Fehler",
  safe_state: "Sicherer Zustand",
};

export const FAULT_LABELS: Record<Fault, string> = {
  none: "keine",
  chamber_unassigned: "Garraum keinem Sensor zugeordnet",
  chamber_missing: "Garraumsensor fehlt",
  chamber_read_error: "Lesefehler am Garraumsensor",
  chamber_duplicate: "ID des Garraumsensors doppelt",
  implausible: "Garraumwert unplausibel",
  over_temperature: "Übertemperatur",
  frozen: "Garraumwert eingefroren",
  cycle_overrun: "Regelzyklus zu langsam",
};

export const ROLE_LABELS: Record<Role, string> = {
  unassigned: "nicht zugeordnet",
  chamber: "Garraum",
  probe_1: "Kern 1",
  probe_2: "Kern 2",
  smoke_gen: "Generator",
};

export const ROLE_STATUS_LABELS: Record<RoleStatus, string> = {
  ok: "in Ordnung",
  unassigned: "nicht zugeordnet",
  missing: "fehlt",
  read_error: "Lesefehler",
  duplicate: "doppelt",
};

export const READ_STATUS_LABELS: Record<ReadStatus, string> = {
  ok: "in Ordnung",
  not_found: "nicht gefunden",
  read_error: "Lesefehler",
};

export const RESULT_LABELS: Record<CommandResult, string> = {
  ok: "Ausgeführt",
  no_setpoint: "Erst einen Sollwert setzen",
  already_running: "Läuft bereits",
  fault_active: "Start gesperrt, ein Fehler ist aktiv",
  safe_state: "Start gesperrt, sicherer Zustand",
  setpoint_too_low: "Sollwert zu niedrig",
  setpoint_too_high: "Sollwert zu hoch",
  setpoint_not_a_number: "Sollwert ist keine Zahl",
  sensors_locked: "Beim Heizen gesperrt",
  unknown_sensor: "Unbekannter Sensor",
  invalid_value: "Ungültiger Wert",
  storage_error: "Speichern fehlgeschlagen, nichts geändert",
  not_in_safe_state: "Kein sicherer Zustand aktiv",
  cause_active: "Die Ursache besteht noch",
  unknown_command: "Unbekannter Befehl",
};

export const ERROR_LABELS: Record<RequestErrorCode, string> = {
  bad_json: "Anfrage kaputt",
  missing_field: "Feld fehlt",
  wrong_type: "Falscher Typ",
  unknown_field: "Unbekanntes Feld",
  unknown_value: "Unbekannter Wert",
  bad_sensor_id: "Ungültige Sensor-ID",
  too_large: "Anfrage zu groß",
  busy: "Gerät beschäftigt, bitte erneut versuchen",
  timeout: "Keine Antwort vom Regeltask, der Befehl kann trotzdem wirken",
  not_ready: "Gerät startet noch",
};

const oneDecimal = new Intl.NumberFormat("de-DE", { minimumFractionDigits: 1, maximumFractionDigits: 1 });

/* "79,9 °C", ohne gültigen Wert ein Gedankenstrich */
export function formatTemperature(tempC: number | null): string {
  return tempC === null ? "–" : `${oneDecimal.format(tempC)} °C`;
}

/* Tastgrad 0 bis 1 als "46 %" */
export function formatDuty(duty: number): string {
  return `${Math.round(duty * 100)} %`;
}
