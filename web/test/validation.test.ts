import { describe, expect, it } from "vitest";

import { checkOffset, checkSetpoint } from "../src/validation";
import { exampleLimits } from "./fixtures";

describe("Prüfung der Eingaben", () => {
  /* WEB-06: ungültige Eingaben */
  it.each([
    ["", "Bitte einen Sollwert eingeben"],
    ["   ", "Bitte einen Sollwert eingeben"],
    ["abc", "Keine gültige Zahl"],
    ["80°", "Keine gültige Zahl"],
    ["1e2", "Keine gültige Zahl"],
    ["8,0,0", "Keine gültige Zahl"],
    ["19,9", "Mindestens 20 °C"],
    ["110.1", "Höchstens 110 °C"],
  ])("lehnt den Sollwert %j ab", (text, message) => {
    expect(checkSetpoint(text, exampleLimits)).toEqual({ ok: false, message });
  });

  it.each([
    ["80", 80],
    ["80,5", 80.5],
    [" 20 ", 20],
    ["110", 110],
  ])("nimmt den Sollwert %j an", (text, value) => {
    expect(checkSetpoint(text, exampleLimits)).toEqual({ ok: true, value });
  });

  it("schickt ohne Grenzen nichts", () => {
    expect(checkSetpoint("80", null).ok).toBe(false);
    expect(checkOffset("0", null).ok).toBe(false);
  });

  it("prüft den Offset gegen ±offset_max_c", () => {
    expect(checkOffset("-5", exampleLimits)).toEqual({ ok: true, value: -5 });
    expect(checkOffset("0,25", exampleLimits)).toEqual({ ok: true, value: 0.25 });
    expect(checkOffset("5,1", exampleLimits)).toEqual({ ok: false, message: "Höchstens ±5 °C" });
    expect(checkOffset("", exampleLimits)).toEqual({ ok: false, message: "Keine gültige Zahl" });
  });
});
