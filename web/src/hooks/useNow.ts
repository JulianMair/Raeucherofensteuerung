import { useEffect, useState } from "preact/hooks";

/* Aktuelle Zeit in ms, neu jede intervalMs. Für "letzte Daten vor 12 s". */
export function useNow(intervalMs: number): number {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    const timer = setInterval(() => setNow(Date.now()), intervalMs);
    return () => clearInterval(timer);
  }, [intervalMs]);
  return now;
}
