// Verlauf für die Livekurve (WEB-02, E-043). Das Gerät speichert keinen
// Verlauf; die Kurve beginnt mit dem Öffnen der Seite und hält höchstens
// HISTORY_MAX_POINTS Punkte. Ältere fallen vorne heraus.
import type { StateSnapshot } from "./api/types";

export const HISTORY_MAX_POINTS = 6 * 3600; /* 6 Stunden bei einem Punkt je Sekunde */

/* Spalten wie uPlot sie braucht: Zeit und je Linie ein Wert, null für Lücken */
export interface History {
  time_s: number[]; /* Uhrzeit des Browsers in Sekunden; die Zeit des Geräts zählt ab seinem Start */
  chamber: (number | null)[];
  probe_1: (number | null)[];
  probe_2: (number | null)[];
  setpoint: (number | null)[];
  lastDeviceMs: number | null; /* Zeit des Geräts beim letzten Punkt */
}

export function emptyHistory(): History {
  return { time_s: [], chamber: [], probe_1: [], probe_2: [], setpoint: [], lastDeviceMs: null };
}

function reading(state: StateSnapshot, role: "chamber" | "probe_1" | "probe_2"): number | null {
  const value = state.roles[role];
  return value.status === "ok" ? value.temp_c : null;
}

/* Hängt einen Zustand an. Verändert history, damit 6 Stunden Verlauf nicht
 * jede Sekunde kopiert werden. Derselbe Zyklus zweimal zählt nur einmal,
 * z. B. die sofortige Nachricht nach einer Wiederverbindung. */
export function appendState(history: History, state: StateSnapshot, nowMs: number, max = HISTORY_MAX_POINTS): boolean {
  if (state.time_ms === history.lastDeviceMs) {
    return false;
  }
  history.lastDeviceMs = state.time_ms;
  history.time_s.push(nowMs / 1000);
  history.chamber.push(reading(state, "chamber"));
  history.probe_1.push(reading(state, "probe_1"));
  history.probe_2.push(reading(state, "probe_2"));
  history.setpoint.push(state.setpoint_c);
  while (history.time_s.length > max) {
    history.time_s.shift();
    history.chamber.shift();
    history.probe_1.shift();
    history.probe_2.shift();
    history.setpoint.shift();
  }
  return true;
}
