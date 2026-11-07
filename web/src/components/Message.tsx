import type { Message as MessageData } from "../outcome";

/* Meldung zum letzten Befehl, Bildschirmleser lesen sie vor */
export function Message({ message }: { message: MessageData | null }) {
  return (
    <p class={message === null ? "message" : message.ok ? "message ok" : "message error"} aria-live="polite" data-testid="message">
      {message?.text ?? ""}
    </p>
  );
}
