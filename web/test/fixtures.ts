// Testdaten aus der API selbst: derselbe Zustand, den der Vertragstest in
// proto gegen die Firmware prüft (test_state_matches_api_example).
import example from "../../docs/api-examples/state.json";
import type { Limits, StateSnapshot } from "../src/api/types";

export const exampleState = example as StateSnapshot;

export const exampleLimits: Limits = {
  setpoint_min_c: 20,
  setpoint_max_c: 110,
  offset_max_c: 5,
  hard_max_c: 120,
  core_target_min_c: 30,
  core_target_max_c: 100,
};

/* Kopie des Beispiels mit Änderungen, das Beispiel selbst bleibt unverändert */
export function stateWith(changes: Partial<StateSnapshot>): StateSnapshot {
  return { ...structuredClone(exampleState), ...changes };
}
