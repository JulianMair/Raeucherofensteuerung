#include "wifi_ap.h"

#include "wifi_credentials.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <string.h>

static const char *const TAG = "wifi";

/* Die DHCP-Option speichert nur den Zeiger, der Text muss bleiben */
static const char CAPTIVE_PORTAL_URI[] = "http://" NET_AP_IP_TEXT "/";

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = data;
        ESP_LOGI(TAG, "Gerät " MACSTR " verbunden", MAC2STR(event->mac));
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = data;
        ESP_LOGI(TAG, "Gerät " MACSTR " getrennt", MAC2STR(event->mac));
    }
}

/* DHCP-Option 114: Das Gerät erfährt schon beim Verbinden die Adresse der Seite */
static esp_err_t announce_captive_portal(esp_netif_t *netif)
{
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_stop(netif), TAG, "DHCP-Server anhalten");
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                               (void *)CAPTIVE_PORTAL_URI, strlen(CAPTIVE_PORTAL_URI)),
                        TAG, "Captive-Portal-Option setzen");
    return esp_netif_dhcps_start(netif);
}

esp_err_t wifi_ap_start(const char *ssid, const char *password)
{
    const net_wifi_check_t check = net_wifi_check(ssid, password);
    if (check != NET_WIFI_OK) {
        ESP_LOGE(TAG, "Access Point startet nicht: %s", net_wifi_check_message(check));
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Netzwerkstapel");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Ereignisschleife");
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(netif != NULL, ESP_FAIL, TAG, "Netzwerkschnittstelle");

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "WLAN-Treiber");
    /* Die Konfiguration nicht im NVS ablegen: Sie kommt bei jedem Start aus
     * Kconfig, und der Flash wird nicht unnötig beschrieben. */
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "WLAN-Speicher");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL), TAG,
                        "WLAN-Ereignisse");

    wifi_config_t config = { 0 };
    const size_t ssid_len = strlen(ssid); /* geprüft: 1 bis 32 */
    memcpy(config.ap.ssid, ssid, ssid_len);
    config.ap.ssid_len = (uint8_t)ssid_len;
    memcpy(config.ap.password, password, strlen(password) + 1); /* geprüft: höchstens 63 */
    config.ap.channel = NET_AP_CHANNEL;
    config.ap.max_connection = NET_AP_MAX_CLIENTS;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "AP-Modus");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "AP-Konfiguration");
    ESP_RETURN_ON_ERROR(announce_captive_portal(netif), TAG, "DHCP");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "WLAN starten");
    ESP_LOGI(TAG, "Access Point \"%s\" läuft, Seite unter http://%s/", ssid, NET_AP_IP_TEXT);
    return ESP_OK;
}
