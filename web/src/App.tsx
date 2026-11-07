import { useEffect, useState } from "preact/hooks";

import { getLimits } from "./api/client";
import { browserSocket, type SocketFactory } from "./api/live";
import type { Limits } from "./api/types";
import { ConnectionBanner } from "./components/ConnectionBanner";
import { Controls } from "./components/Controls";
import { StatusPanel } from "./components/StatusPanel";
import { useLive } from "./hooks/useLive";
import { useNow } from "./hooks/useNow";

interface Props {
  /* Die Tests setzen hier einen Fake-WebSocket ein */
  createSocket?: SocketFactory;
}

export function App({ createSocket = browserSocket }: Props) {
  const live = useLive(createSocket);
  const now = useNow(1000);
  const [limits, setLimits] = useState<Limits | null>(null);
  const stale = live.status !== "open";

  /* Grenzen für die Prüfung der Eingaben, sobald das Gerät erreichbar ist */
  useEffect(() => {
    if (live.status === "open" && limits === null) {
      void getLimits().then(setLimits);
    }
  }, [live.status, limits]);

  return (
    <main>
      <h1>Räucherofen</h1>
      <ConnectionBanner live={live} now={now} />
      {live.state !== null && (
        <>
          <StatusPanel state={live.state} stale={stale} />
          <Controls state={live.state.state} limits={limits} disabled={stale} />
        </>
      )}
    </main>
  );
}
