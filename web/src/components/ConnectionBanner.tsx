import type { Live } from "../hooks/useLive";

interface Props {
  live: Live;
  now: number;
}

/* WEB-03: Ohne Verbindung sieht man sofort, wie alt die gezeigten Werte sind */
export function ConnectionBanner({ live, now }: Props) {
  if (live.status === "open" && live.state !== null) {
    return null;
  }
  if (live.receivedAt === null) {
    return (
      <p class="banner banner-connecting" role="status">
        Verbinde mit dem Räucherofen …
      </p>
    );
  }
  const ageS = Math.max(0, Math.round((now - live.receivedAt) / 1000));
  return (
    <p class="banner banner-offline" role="status">
      Verbindung getrennt, letzte Daten vor {ageS} s. Neuer Versuch läuft.
    </p>
  );
}
