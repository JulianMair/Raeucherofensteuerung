import { browserSocket, type SocketFactory } from "./api/live";
import { ConnectionBanner } from "./components/ConnectionBanner";
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
  const stale = live.status !== "open";
  return (
    <main>
      <h1>Räucherofen</h1>
      <ConnectionBanner live={live} now={now} />
      {live.state !== null && <StatusPanel state={live.state} stale={stale} />}
    </main>
  );
}
