/*
 * Livewerte über den WebSocket /ws (API-03, E-039).
 *
 * Jede Sekunde bekommt jeder verbundene Client den Zustand als Text, dasselbe
 * JSON wie GET /api/state. Ein neuer Client bekommt ihn sofort. Ein kleiner
 * Task stößt das nur an; kodiert und gesendet wird im Task des HTTP-Servers
 * (httpd_queue_work). So benutzt nur ein Task den Puffer. Nachrichten vom
 * Client werden gelesen und verworfen.
 */
#ifndef WS_LIVE_H
#define WS_LIVE_H

#include "snapshot_store.h"

#include "esp_err.h"
#include "esp_http_server.h"

#define WS_LIVE_PERIOD_MS 1000u
#define WS_LIVE_MAX_CLIENTS 4
#define WS_LIVE_PRIORITY 3u /* unter HTTP (5) und DNS (4) */
#define WS_LIVE_STACK_BYTES 2048u
#define WS_LIVE_CORE 0

/* Meldet /ws an und startet den Task. Die Zeiger müssen gültig bleiben. */
esp_err_t ws_live_start(httpd_handle_t server, snapshot_store_t *store);

#endif /* WS_LIVE_H */
