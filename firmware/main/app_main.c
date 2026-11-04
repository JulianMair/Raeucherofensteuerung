/*
 * Einstieg der Firmware. ESP-IDF ruft app_main() auf, nachdem FreeRTOS läuft.
 */
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Raeucherofensteuerung startet");
}
