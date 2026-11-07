#include "dns_server.h"

#include "dns_message.h"
#include "wifi_ap.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include <stdint.h>

static const char *const TAG = "dns";
static const uint8_t AP_IP[4] = NET_AP_IP_BYTES;

static StaticTask_t task_buffer;
static StackType_t task_stack[DNS_SERVER_STACK_BYTES];
/* Nur der DNS-Task benutzt die Puffer */
static uint8_t query[DNS_MESSAGE_MAX];
static uint8_t response[DNS_MESSAGE_MAX];

static void dns_server_main(void *arg)
{
    const int sock = (int)(intptr_t)arg;
    for (;;) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        const int len = recvfrom(sock, query, sizeof(query), 0, (struct sockaddr *)&client, &client_len);
        if (len < 0) {
            ESP_LOGW(TAG, "Empfang fehlgeschlagen (errno %d)", errno);
            vTaskDelay(pdMS_TO_TICKS(100)); /* nicht im Kreis drehen */
            continue;
        }
        /* Kaputte Anfragen bleiben unbeantwortet */
        const size_t answer_len = dns_build_answer(query, (size_t)len, AP_IP, response, sizeof(response));
        if (answer_len > 0) {
            sendto(sock, response, answer_len, 0, (const struct sockaddr *)&client, client_len);
        }
    }
}

esp_err_t dns_server_start(void)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Kein UDP-Socket (errno %d)", errno);
        return ESP_FAIL;
    }
    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr = { .s_addr = htonl(INADDR_ANY) },
    };
    if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "Port %d nicht verfügbar (errno %d)", DNS_PORT, errno);
        close(sock);
        return ESP_FAIL;
    }
    xTaskCreateStaticPinnedToCore(dns_server_main, "dns", DNS_SERVER_STACK_BYTES, (void *)(intptr_t)sock,
                                  DNS_SERVER_PRIORITY, task_stack, &task_buffer, DNS_SERVER_CORE);
    ESP_LOGI(TAG, "DNS-Server läuft, jeder Name zeigt auf %s", NET_AP_IP_TEXT);
    return ESP_OK;
}
