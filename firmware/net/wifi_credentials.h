/*
 * Prüfung von SSID und WLAN-Passwort, bevor der Access Point startet
 * (NET-01, NET-04, E-041). Reines C, damit die Host-Tests es prüfen.
 */
#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

/* Standard des Passworts in Kconfig.projbuild. Er steht öffentlich im
 * Repository, mit ihm startet der Access Point nicht. Muss dem Standard in
 * Kconfig entsprechen, das prüft der ctest kconfig_placeholder. */
#define NET_WIFI_PASSWORD_PLACEHOLDER "bitte-aendern"
#define NET_WIFI_PASSWORD_MIN 8u  /* WPA2 */
#define NET_WIFI_PASSWORD_MAX 63u /* WPA2 als Text */
#define NET_WIFI_SSID_MAX 32u

typedef enum {
    NET_WIFI_OK = 0,
    NET_WIFI_SSID_INVALID,         /* leer oder länger als 32 Byte */
    NET_WIFI_PASSWORD_IS_PLACEHOLDER, /* noch der Standard aus dem Repository */
    NET_WIFI_PASSWORD_LENGTH       /* nicht 8 bis 63 Zeichen */
} net_wifi_check_t;

net_wifi_check_t net_wifi_check(const char *ssid, const char *password);

/* Meldung für das Log, auch für Werte außerhalb der Liste */
const char *net_wifi_check_message(net_wifi_check_t check);

#endif /* WIFI_CREDENTIALS_H */
