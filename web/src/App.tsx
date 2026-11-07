import { useEffect, useRef, useState } from "preact/hooks";

import { getLimits } from "./api/client";
import { browserSocket, type SocketFactory } from "./api/live";
import type { Limits } from "./api/types";
import { ConnectionBanner } from "./components/ConnectionBanner";
import { Controls } from "./components/Controls";
import { LiveChart } from "./components/LiveChart";
import { SensorList } from "./components/SensorList";
import { StatusPanel } from "./components/StatusPanel";
import { appendState, emptyHistory } from "./history";
import { useLive } from "./hooks/useLive";
import { useNow } from "./hooks/useNow";

interface Props {
  /* Die Tests setzen hier einen Fake-WebSocket ein */
  createSocket?: SocketFactory;
}

export function App({ createSocket = browserSocket }: Props) {
  /* Der Verlauf liegt außerhalb des Zustands von Preact, version zeichnet neu */
  const history = useRef(emptyHistory());
  const [version, setVersion] = useState(0);
  const live = useLive(createSocket, (state) => {
    if (appendState(history.current, state, Date.now())) {
      setVersion((v) => v + 1);
    }
  });
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
          <LiveChart history={history.current} version={version} />
          <SensorList
            channels={live.state.channels}
            overflow={live.state.sensor_overflow}
            limits={limits}
            locked={live.state.state === "heating" || live.state.state === "holding"}
            disabled={stale}
          />
        </>
      )}
    </main>
  );
}
