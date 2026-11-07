// Läuft vor jeder Testdatei. Ohne globale Testfunktionen räumt Testing
// Library nicht von selbst auf, deshalb hier nach jedem Test. MSW ersetzt
// die REST-API des Geräts (WEB-05), ein Fake ersetzt uPlot.
import { cleanup } from "@testing-library/preact";
import { afterAll, afterEach, beforeAll, vi } from "vitest";

import { FakePlot } from "./fakeUplot";
import { requests, server } from "./server";

/* jsdom hat kein Canvas, uPlot wird in allen Tests ersetzt */
vi.mock("uplot", () => ({ default: FakePlot }));

beforeAll(() => {
  server.listen({ onUnhandledRequest: "error" });
});

afterEach(() => {
  cleanup();
  server.resetHandlers();
  requests.length = 0;
  FakePlot.instances.length = 0;
});

afterAll(() => {
  server.close();
});
