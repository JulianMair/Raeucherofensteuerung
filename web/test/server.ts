// MSW als Ersatz für die REST-API des Geräts. Jede Anfrage ohne passenden
// Handler lässt den Test scheitern, und requests zeigt, was gesendet wurde.
import { http, HttpResponse } from "msw";
import { setupServer } from "msw/node";

import { exampleLimits, exampleState } from "./fixtures";

export interface SentRequest {
  method: string;
  path: string;
  body: unknown;
}

export const requests: SentRequest[] = [];

export const server = setupServer(
  http.get("/api/config", () => HttpResponse.json(exampleLimits)),
  http.get("/api/state", () => HttpResponse.json(exampleState)),
);

server.events.on("request:start", ({ request }) => {
  const path = new URL(request.url).pathname;
  void request
    .clone()
    .text()
    .then((text) => {
      requests.push({ method: request.method, path, body: text === "" ? undefined : JSON.parse(text) });
    });
});
