import { useEffect, useState } from "preact/hooks";

import { connectLive, type LiveStatus, type SocketFactory, wsUrl } from "../api/live";
import type { StateSnapshot } from "../api/types";

export interface Live {
  status: LiveStatus;
  state: StateSnapshot | null; /* letzter Zustand, auch nach einem Abbruch */
  receivedAt: number | null;   /* Date.now() beim letzten Zustand */
}

/* Hält die Verbindung zu /ws, solange die Komponente da ist (API-03, WEB-03) */
export function useLive(createSocket: SocketFactory, onState?: (state: StateSnapshot) => void): Live {
  const [live, setLive] = useState<Live>({ status: "connecting", state: null, receivedAt: null });
  useEffect(
    () =>
      connectLive(createSocket, wsUrl(window.location), {
        onState: (state) => {
          setLive((previous) => ({ ...previous, state, receivedAt: Date.now() }));
          onState?.(state);
        },
        onStatus: (status) => setLive((previous) => ({ ...previous, status })),
      }),
    // onState gehört bewusst nicht dazu: Eine neue Funktion soll keine neue Verbindung öffnen
    [createSocket],
  );
  return live;
}
