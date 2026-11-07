import type { StateSnapshot } from "../api/types";
import { FAULT_LABELS, formatDuty, formatTemperature, ROLE_LABELS, ROLE_STATUS_LABELS, STATE_LABELS } from "../labels";

interface Props {
  state: StateSnapshot;
  stale: boolean; /* Werte sind alt, die Verbindung ist weg */
}

/* Rollen mit Messwert; "nicht zugeordnet" hat keinen */
type ReadingRole = keyof StateSnapshot["roles"];
const SHOWN_ROLES: ReadingRole[] = ["chamber", "probe_1", "probe_2", "smoke_gen"];

/* Fehler und sicherer Zustand stehen als Band ganz oben, mit Ursache */
function FaultBand({ state }: { state: StateSnapshot }) {
  if (state.state === "safe_state") {
    return (
      <div class="band band-safe" role="alert">
        <strong>Sicherer Zustand: Heizung aus.</strong> Ursache: {FAULT_LABELS[state.latched_fault]}. Erst
        zurücksetzen, wenn die Ursache behoben ist.
      </div>
    );
  }
  if (state.state === "fault") {
    return (
      <div class="band band-fault" role="alert">
        <strong>Fehler: Start gesperrt.</strong> Ursache: {FAULT_LABELS[state.active_fault]}.
      </div>
    );
  }
  return null;
}

export function StatusPanel({ state, stale }: Props) {
  return (
    <section class={stale ? "panel stale" : "panel"} aria-label="Status">
      <FaultBand state={state} />
      <p class={`state state-${state.state}`} data-testid="state">
        {STATE_LABELS[state.state]}
      </p>
      <dl class="figures">
        <dt>Sollwert</dt>
        <dd>{formatTemperature(state.setpoint_c)}</dd>
        <dt>Heizung</dt>
        <dd>{formatDuty(state.duty)}</dd>
      </dl>
      <dl class="readings">
        {SHOWN_ROLES.map((role) => {
          const reading = state.roles[role];
          return [
            <dt key={`${role}-name`}>{ROLE_LABELS[role]}</dt>,
            <dd key={`${role}-value`} data-testid={`reading-${role}`}>
              {reading.status === "ok" ? formatTemperature(reading.temp_c) : ROLE_STATUS_LABELS[reading.status]}
            </dd>,
          ];
        })}
      </dl>
    </section>
  );
}
