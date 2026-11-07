import { useState } from "preact/hooks";

import { type CommandOutcome, setOffset, setRole } from "../api/client";
import type { Channel, Limits, Role } from "../api/types";
import { formatTemperature, READ_STATUS_LABELS, ROLE_LABELS } from "../labels";
import { describeOutcome, type Message as MessageData } from "../outcome";
import { checkOffset } from "../validation";
import { Message } from "./Message";

interface Props {
  channels: Channel[];
  overflow: boolean;
  limits: Limits | null;
  locked: boolean;   /* beim Heizen gesperrt (E-022) */
  disabled: boolean; /* ohne Verbindung keine Befehle (WEB-03) */
}

const ALL_ROLES = Object.keys(ROLE_LABELS) as Role[];
const oneDecimal = new Intl.NumberFormat("de-DE", { minimumFractionDigits: 1, maximumFractionDigits: 2 });

function channelStatus(channel: Channel): string {
  if (channel.duplicate) {
    return "doppelt";
  }
  return channel.present ? READ_STATUS_LABELS[channel.read_status] : "nicht gefunden";
}

interface RowProps {
  channel: Channel;
  limits: Limits | null;
  locked: boolean;
  run: (command: () => Promise<CommandOutcome>) => void;
  report: (message: MessageData) => void;
}

function SensorRow({ channel, limits, locked, run, report }: RowProps) {
  const [offset, setOffsetText] = useState(oneDecimal.format(channel.offset_c));

  const submitOffset = (event: Event) => {
    event.preventDefault();
    const checked = checkOffset(offset, limits);
    if (!checked.ok) {
      report({ text: checked.message, ok: false });
      return;
    }
    run(() => setOffset(channel.id, checked.value));
  };

  return (
    <li class="sensor">
      <p class="sensor-head">
        <code>{channel.id}</code> <span>{channel.type}</span>
      </p>
      <p class="sensor-value">
        {formatTemperature(channel.temp_c)} <span class="sensor-status">{channelStatus(channel)}</span>
      </p>
      <label>
        Rolle von {channel.id}
        <select
          value={channel.role}
          disabled={locked}
          onChange={(event) => {
            const role = event.currentTarget.value as Role;
            run(() => setRole(channel.id, role));
          }}
        >
          {ALL_ROLES.map((role) => (
            <option key={role} value={role}>
              {ROLE_LABELS[role]}
            </option>
          ))}
        </select>
      </label>
      <form class="offset" onSubmit={submitOffset}>
        <label>
          Offset von {channel.id} (°C)
          <input
            type="text"
            inputMode="decimal"
            autoComplete="off"
            value={offset}
            disabled={locked}
            onInput={(event) => setOffsetText(event.currentTarget.value)}
          />
        </label>
        <button type="submit" disabled={locked}>
          Übernehmen
        </button>
      </form>
    </li>
  );
}

/* Sensoren mit Rolle und Kalibrieroffset (WEB-02, SEN-01, SEN-02, E-044) */
export function SensorList({ channels, overflow, limits, locked, disabled }: Props) {
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState<MessageData | null>(null);

  const run = (command: () => Promise<CommandOutcome>) => {
    setBusy(true);
    void command().then((outcome) => {
      setMessage(describeOutcome(outcome));
      setBusy(false);
    });
  };

  const blocked = locked || disabled || busy;
  return (
    <section class="panel" aria-label="Sensoren">
      <h2>Sensoren</h2>
      {locked && <p class="hint">Rollen und Offsets sind beim Heizen gesperrt.</p>}
      {overflow && <p class="hint">Mehr Sensoren am Bus, als es Kanäle gibt. Nicht alle werden angezeigt.</p>}
      {channels.length === 0 ? (
        <p class="hint">Keine Sensoren gefunden.</p>
      ) : (
        <ul class="sensors">
          {channels.map((channel) => (
            <SensorRow key={channel.id} channel={channel} limits={limits} locked={blocked} run={run} report={setMessage} />
          ))}
        </ul>
      )}
      <Message message={message} />
    </section>
  );
}
