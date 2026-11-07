/*
 * Access Point mit WPA2 (NET-01, NET-02, NET-04, E-041). Kein Router und
 * kein Internet nötig. ESP-IDF gibt dem Access Point die Adresse
 * 192.168.4.1, der DHCP-Server verteilt Adressen und meldet die Seite als
 * Captive Portal (DHCP-Option 114, RFC 8910).
 */
#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "esp_err.h"

/* Standardadresse von ESP-IDF für den Access Point, als Text und als Bytes */
#define NET_AP_IP_TEXT "192.168.4.1"
#define NET_AP_IP_BYTES { 192, 168, 4, 1 }
#define NET_AP_CHANNEL 1
#define NET_AP_MAX_CLIENTS 4

/* Prüft SSID und Passwort (wifi_credentials) und startet den Access Point.
 * ESP_ERR_INVALID_ARG ohne Start, wenn die Prüfung scheitert; der Grund
 * steht im Log. */
esp_err_t wifi_ap_start(const char *ssid, const char *password);

#endif /* WIFI_AP_H */
