import { useState } from "preact/hooks";

import { type CommandOutcome, reset, setCoreTarget, setSetpoint, start, stop } from "../api/client";
import type { Limits, State } from "../api/types";
import { describeOutcome, type Message as MessageData } from "../outcome";
import { type Checked, checkCoreTarget, checkSetpoint } from "../validation";
import { Message } from "./Message";

interface Props {
  state: State;
  limits: Limits | null;
  disabled: boolean; /* ohne Verbindung keine Befehle (WEB-03) */
}

/* Sollwert, Kernziel, Start, Stopp und Reset (WEB-02, FC-01) */
export function Controls({ state, limits, disabled }: Props) {
  const [setpoint, setSetpointText] = useState("");
  const [coreTarget, setCoreTargetText] = useState("");
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState<MessageData | null>(null);

  const run = async (command: () => Promise<CommandOutcome>) => {
    setBusy(true);
    setMessage(describeOutcome(await command()));
    setBusy(false);
  };

  /* Erst prüfen, nur Gültiges senden (WEB-06) */
  const submit = (event: Event, checked: Checked, command: (value: number) => Promise<CommandOutcome>) => {
    event.preventDefault();
    if (!checked.ok) {
      setMessage({ text: checked.message, ok: false });
      return;
    }
    void run(() => command(checked.value));
  };

  const locked = disabled || busy;
  const running = state === "heating" || state === "holding";
  return (
    <section class="panel" aria-label="Bedienung">
      <form class="setpoint" onSubmit={(event) => submit(event, checkSetpoint(setpoint, limits), setSetpoint)}>
        <label for="setpoint">Sollwert (°C)</label>
        <input
          id="setpoint"
          type="text"
          inputMode="decimal"
          autoComplete="off"
          value={setpoint}
          onInput={(event) => setSetpointText(event.currentTarget.value)}
          disabled={locked}
        />
        <button type="submit" disabled={locked}>
          Setzen
        </button>
      </form>
      {/* Das Kernziel wirkt nicht auf die Heizung, deshalb in jedem Zustand erlaubt */}
      <form class="setpoint" onSubmit={(event) => submit(event, checkCoreTarget(coreTarget, limits), setCoreTarget)}>
        <label for="core-target">Kernziel (°C)</label>
        <input
          id="core-target"
          type="text"
          inputMode="decimal"
          autoComplete="off"
          value={coreTarget}
          onInput={(event) => setCoreTargetText(event.currentTarget.value)}
          disabled={locked}
        />
        <button type="submit" disabled={locked} aria-label="Kernziel setzen">
          Setzen
        </button>
      </form>
      <div class="buttons">
        <button type="button" onClick={() => void run(start)} disabled={locked || running || state === "safe_state"}>
          Start
        </button>
        <button type="button" onClick={() => void run(stop)} disabled={locked || !running}>
          Stopp
        </button>
        <button type="button" class="reset" onClick={() => void run(reset)} disabled={locked || state !== "safe_state"}>
          Sicheren Zustand zurücksetzen
        </button>
      </div>
      <Message message={message} />
    </section>
  );
}
