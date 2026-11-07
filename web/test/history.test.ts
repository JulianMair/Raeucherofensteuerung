import { describe, expect, it } from "vitest";

import { appendState, emptyHistory } from "../src/history";
import { stateWith } from "./fixtures";

describe("Verlauf der Livekurve", () => {
  it("hängt Zeit, Rollen und Sollwert an, ohne gültigen Wert eine Lücke", () => {
    const history = emptyHistory();
    expect(appendState(history, stateWith({ time_ms: 1000 }), 1_700_000_000_000)).toBe(true);

    expect(history.time_s).toEqual([1_700_000_000]);
    expect(history.chamber).toEqual([79.94]);
    expect(history.probe_1).toEqual([61.5]);
    expect(history.probe_2).toEqual([null]); /* nicht zugeordnet */
    expect(history.setpoint).toEqual([80]);
  });

  it("zeigt ohne Sollwert eine Lücke", () => {
    const history = emptyHistory();
    appendState(history, stateWith({ setpoint_c: null }), 0);
    expect(history.setpoint).toEqual([null]);
  });

  /* Nach einer Wiederverbindung kommt derselbe Zyklus ein zweites Mal */
  it("zählt denselben Zyklus nur einmal", () => {
    const history = emptyHistory();
    appendState(history, stateWith({ time_ms: 5000 }), 0);
    expect(appendState(history, stateWith({ time_ms: 5000 }), 1000)).toBe(false);
    expect(history.time_s).toHaveLength(1);
  });

  it("vergisst die ältesten Punkte über der Höchstzahl", () => {
    const history = emptyHistory();
    for (let i = 0; i < 5; i++) {
      appendState(history, stateWith({ time_ms: i * 1000 }), i * 1000, 3);
    }
    expect(history.time_s).toEqual([2, 3, 4]);
    expect(history.chamber).toHaveLength(3);
    expect(history.setpoint).toHaveLength(3);
  });
});
