// Text zum Ergebnis eines Befehls (WEB-06): angenommen, mit Grund abgelehnt,
// oder schon vor dem Regeltask gescheitert.
import type { CommandOutcome } from "./api/client";
import { ERROR_LABELS, RESULT_LABELS } from "./labels";

export interface Message {
  text: string;
  ok: boolean;
}

export function describeOutcome(outcome: CommandOutcome): Message {
  switch (outcome.kind) {
    case "done":
      return { text: RESULT_LABELS[outcome.result], ok: outcome.result === "ok" };
    case "error": {
      const field = outcome.error.field === undefined ? "" : `: ${outcome.error.field}`;
      return { text: `${ERROR_LABELS[outcome.error.error]}${field}`, ok: false };
    }
    case "network":
      return { text: "Keine Verbindung zum Gerät", ok: false };
  }
}
