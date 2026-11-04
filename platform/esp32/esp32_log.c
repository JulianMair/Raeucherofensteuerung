#include "esp32_log.h"

#include "esp_log.h"

static const char *const TAG = "core";

static void log_write(void *ctx, rof_log_level_t level, const char *msg)
{
    (void)ctx;
    switch (level) {
    case ROF_LOG_ERROR:
        ESP_LOGE(TAG, "%s", msg);
        break;
    case ROF_LOG_WARN:
        ESP_LOGW(TAG, "%s", msg);
        break;
    case ROF_LOG_INFO:
        ESP_LOGI(TAG, "%s", msg);
        break;
    default:
        ESP_LOGD(TAG, "%s", msg);
        break;
    }
}

rof_log_t esp32_log_interface(void)
{
    return (rof_log_t){ .ctx = NULL, .write = log_write };
}
