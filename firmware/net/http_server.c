#include "http_server.h"

#include "api_handlers.h"
#include "request_path.h"
#include "wifi_ap.h"
#include "ws_live.h"

#include "esp_http_server.h"
#include "esp_littlefs.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *const TAG = "http";
static const char START_PAGE[] = "http://" NET_AP_IP_TEXT "/";

static httpd_handle_t server;
/* Nur der eine Task des Servers benutzt den Puffer */
static char chunk[1024];

static esp_err_t mount_web_files(void)
{
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = NET_WWW_ROOT,
        .partition_label = "www",
        .format_if_mount_failed = false, /* nie das Abbild überschreiben */
        .read_only = true,
    };
    return esp_vfs_littlefs_register(&conf);
}

static esp_err_t redirect_to_start_page(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", START_PAGE);
    return httpd_resp_send(req, NULL, 0);
}

/* GET auf alles außer der API: Datei aus LittleFS oder Umleitung */
static esp_err_t static_file_handler(httpd_req_t *req)
{
    if (net_is_api_path(req->uri)) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unbekannter API-Pfad");
    }
    net_static_file_t file;
    if (!net_static_file_for(req->uri, &file)) {
        return redirect_to_start_page(req);
    }
    FILE *f = fopen(file.path, "rb");
    if (f == NULL) {
        /* Fehlt sogar die Startseite, nicht umleiten: Das wäre ein Kreis */
        if (strcmp(file.path, NET_WWW_ROOT "/index.html.gz") == 0) {
            return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Weboberfläche fehlt in LittleFS");
        }
        return redirect_to_start_page(req);
    }
    httpd_resp_set_type(req, file.content_type);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, (ssize_t)n) != ESP_OK) {
            fclose(f);
            return ESP_FAIL; /* Verbindung weg, der Server schließt sie */
        }
    }
    fclose(f);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t http_server_start(command_queue_t *queue, snapshot_store_t *store)
{
    const esp_err_t mounted = mount_web_files();
    if (mounted != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS nicht eingehängt (%s), keine Weboberfläche", esp_err_to_name(mounted));
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = HTTP_SERVER_PRIORITY;
    config.core_id = HTTP_SERVER_CORE;
    config.stack_size = HTTP_SERVER_STACK_BYTES;
    config.max_open_sockets = HTTP_SERVER_MAX_SOCKETS;
    config.lru_purge_enable = true;  /* bei vollen Sockets die älteste Verbindung schließen */
    config.send_wait_timeout = 2;    /* s; ein langsamer Client hält den Server nicht lange auf */
    config.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP-Server startet nicht (%s)", esp_err_to_name(err));
        return err;
    }

    /* Reihenfolge zählt: Der Server nimmt den ersten passenden Eintrag.
     * Die Dateien kommen deshalb zuletzt. */
    err = api_handlers_register(server, queue, store);
    if (err != ESP_OK) {
        return err;
    }
    err = ws_live_start(server, store);
    if (err != ESP_OK) {
        return err;
    }
    const httpd_uri_t files = { .uri = "/*", .method = HTTP_GET, .handler = static_file_handler };
    err = httpd_register_uri_handler(server, &files);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "HTTP-Server läuft auf Port %d", config.server_port);
    return ESP_OK;
}
