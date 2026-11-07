// Prüfung der Eingaben vor dem Senden (WEB-06, SAF-09, E-043). Das Gerät
// prüft trotzdem selbst; hier geht es nur darum, offensichtlich Falsches
// gar nicht erst zu schicken. Komma und Punkt sind beide erlaubt.
import type { Limits } from "./api/types";

export type Checked = { ok: true; value: number } | { ok: false; message: string };

const NUMBER = /^-?\d+([.,]\d+)?$/;
const oneDecimal = new Intl.NumberFormat("de-DE", { maximumFractionDigits: 1 });

function parseNumber(text: string): number | null {
  const trimmed = text.trim();
  return NUMBER.test(trimmed) ? Number(trimmed.replace(",", ".")) : null;
}

export function checkSetpoint(text: string, limits: Limits | null): Checked {
  if (limits === null) {
    return { ok: false, message: "Grenzen noch nicht geladen" };
  }
  if (text.trim() === "") {
    return { ok: false, message: "Bitte einen Sollwert eingeben" };
  }
  const value = parseNumber(text);
  if (value === null) {
    return { ok: false, message: "Keine gültige Zahl" };
  }
  if (value < limits.setpoint_min_c) {
    return { ok: false, message: `Mindestens ${oneDecimal.format(limits.setpoint_min_c)} °C` };
  }
  if (value > limits.setpoint_max_c) {
    return { ok: false, message: `Höchstens ${oneDecimal.format(limits.setpoint_max_c)} °C` };
  }
  return { ok: true, value };
}

export function checkOffset(text: string, limits: Limits | null): Checked {
  if (limits === null) {
    return { ok: false, message: "Grenzen noch nicht geladen" };
  }
  const value = parseNumber(text);
  if (value === null) {
    return { ok: false, message: "Keine gültige Zahl" };
  }
  if (Math.abs(value) > limits.offset_max_c) {
    return { ok: false, message: `Höchstens ±${oneDecimal.format(limits.offset_max_c)} °C` };
  }
  return { ok: true, value };
}
