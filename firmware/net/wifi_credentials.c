#include "wifi_credentials.h"

#include <string.h>

net_wifi_check_t net_wifi_check(const char *ssid, const char *password)
{
    const size_t ssid_len = strlen(ssid);
    if (ssid_len == 0 || ssid_len > NET_WIFI_SSID_MAX) {
        return NET_WIFI_SSID_INVALID;
    }
    if (strcmp(password, NET_WIFI_PASSWORD_PLACEHOLDER) == 0) {
        return NET_WIFI_PASSWORD_IS_PLACEHOLDER;
    }
    const size_t password_len = strlen(password);
    if (password_len < NET_WIFI_PASSWORD_MIN || password_len > NET_WIFI_PASSWORD_MAX) {
        return NET_WIFI_PASSWORD_LENGTH;
    }
    return NET_WIFI_OK;
}

const char *net_wifi_check_message(net_wifi_check_t check)
{
    switch (check) {
    case NET_WIFI_OK:
        return "SSID und Passwort in Ordnung";
    case NET_WIFI_SSID_INVALID:
        return "SSID muss 1 bis 32 Zeichen haben";
    case NET_WIFI_PASSWORD_IS_PLACEHOLDER:
        return "WLAN-Passwort ist noch der Platzhalter, bitte in idf.py menuconfig setzen";
    case NET_WIFI_PASSWORD_LENGTH:
        return "WLAN-Passwort muss 8 bis 63 Zeichen haben";
    default:
        return "unbekanntes Ergebnis der Prüfung";
    }
}
