#include "ws_live.h"

#include "http_server.h"
#include "rof_proto.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *const TAG = "ws";

static httpd_handle_t ws_server;
static snapshot_store_t *ws_store;
static StaticTask_t task_buffer;
static StackType_t task_stack[WS_LIVE_STACK_BYTES];
/* Nur der Task des HTTP-Servers benutzt die folgenden Puffer */
static rof_snapshot_t snapshot;
static char json[ROF_PROTO_STATE_JSON_MAX];
static uint8_t discard[128];

/* Kodiert den aktuellen Zustand nach json. false, wenn es noch keinen gibt. */
static bool encode_state(httpd_ws_frame_t *frame)
{
    size_t len = 0;
    if (!snapshot_store_read(ws_store, &snapshot, pdMS_TO_TICKS(10)) ||
        !rof_proto_encode_state(&snapshot, json, sizeof(json), &len)) {
        return false;
    }
    *frame = (httpd_ws_frame_t){ .final = true, .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)json, .len = len };
    return true;
}

static size_t websocket_clients(int *fds, size_t max)
{
    int all[HTTP_SERVER_MAX_SOCKETS];
    size_t count = HTTP_SERVER_MAX_SOCKETS;
    if (httpd_get_client_list(ws_server, &count, all) != ESP_OK) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < count && n < max; i++) {
        if (httpd_ws_get_fd_info(ws_server, all[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            fds[n++] = all[i];
        }
    }
    return n;
}

/* Läuft im Task des HTTP-Servers */
static void push_to_all(void *arg)
{
    (void)arg;
    int fds[HTTP_SERVER_MAX_SOCKETS];
    const size_t n = websocket_clients(fds, HTTP_SERVER_MAX_SOCKETS);
    httpd_ws_frame_t frame;
    if (n == 0 || !encode_state(&frame)) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        httpd_ws_send_frame_async(ws_server, fds[i], &frame);
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* Handshake fertig. Zu viele Clients: diesen gleich wieder schließen */
        int fds[HTTP_SERVER_MAX_SOCKETS];
        if (websocket_clients(fds, HTTP_SERVER_MAX_SOCKETS) > WS_LIVE_MAX_CLIENTS) {
            ESP_LOGW(TAG, "Mehr als %d Clients, Verbindung abgelehnt", WS_LIVE_MAX_CLIENTS);
            return ESP_FAIL;
        }
        httpd_ws_frame_t frame;
        return encode_state(&frame) ? httpd_ws_send_frame(req, &frame) : ESP_OK;
    }
    /* Nachricht vom Client: lesen und verwerfen. Größere schließen die Verbindung. */
    httpd_ws_frame_t frame = { .payload = discard };
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK || frame.len > sizeof(discard)) {
        return ESP_FAIL;
    }
    return frame.len > 0 ? httpd_ws_recv_frame(req, &frame, frame.len) : ESP_OK;
}

static void ws_live_main(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(WS_LIVE_PERIOD_MS));
        httpd_queue_work(ws_server, push_to_all, NULL);
    }
}

esp_err_t ws_live_start(httpd_handle_t server, snapshot_store_t *store)
{
    ws_server = server;
    ws_store = store;
    const httpd_uri_t ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };
    const esp_err_t err = httpd_register_uri_handler(server, &ws);
    if (err != ESP_OK) {
        return err;
    }
    xTaskCreateStaticPinnedToCore(ws_live_main, "ws_live", WS_LIVE_STACK_BYTES, NULL, WS_LIVE_PRIORITY, task_stack,
                                  &task_buffer, WS_LIVE_CORE);
    return ESP_OK;
}
