// Läuft vor jeder Testdatei. Ohne globale Testfunktionen räumt Testing
// Library nicht von selbst auf, deshalb hier nach jedem Test. MSW ersetzt
// die REST-API des Geräts (WEB-05).
import { cleanup } from "@testing-library/preact";
import { afterAll, afterEach, beforeAll } from "vitest";

import { requests, server } from "./server";

beforeAll(() => {
  server.listen({ onUnhandledRequest: "error" });
});

afterEach(() => {
  cleanup();
  server.resetHandlers();
  requests.length = 0;
});

afterAll(() => {
  server.close();
});
