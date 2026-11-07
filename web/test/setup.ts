// Läuft vor jeder Testdatei. Ohne globale Testfunktionen räumt Testing
// Library nicht von selbst auf, deshalb hier nach jedem Test.
import { cleanup } from "@testing-library/preact";
import { afterEach } from "vitest";

afterEach(() => {
  cleanup();
});
