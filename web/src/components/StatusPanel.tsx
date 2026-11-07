import type { Forecast, StateSnapshot } from "../api/types";
import {
  FAULT_LABELS,
  FORECAST_LABELS,
  formatDuration,
  formatDuty,
  formatTemperature,
  ROLE_LABELS,
  ROLE_STATUS_LABELS,
  STATE_LABELS,
} from "../labels";

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

/* Prognose eines Kerns: Restzeit oder warum es keine gibt (FC-01) */
function forecastText(forecast: Forecast): string {
  if (forecast.status === "ok" && forecast.remaining_s !== null) {
    return `noch ca. ${formatDuration(forecast.remaining_s)}`;
  }
  return FORECAST_LABELS[forecast.status];
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
      <dl class="forecast" aria-label="Fertigzeit">
        <dt>Kernziel</dt>
        <dd>{formatTemperature(state.core_target_c)}</dd>
        <dt>Fertig Kern 1</dt>
        <dd data-testid="forecast-probe_1">{forecastText(state.forecast.probe_1)}</dd>
        <dt>Fertig Kern 2</dt>
        <dd data-testid="forecast-probe_2">{forecastText(state.forecast.probe_2)}</dd>
      </dl>
    </section>
  );
}
