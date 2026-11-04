#include "esp32_led_output.h"

#include "driver/gpio.h"

static void on_tick(void *arg)
{
    esp32_led_output_t *led = arg;
    const uint64_t now_ms = rof_clock_now_ms(led->clk);
    portENTER_CRITICAL(&led->lock);
    const bool on = led_pwm_is_on(&led->pwm, now_ms);
    portEXIT_CRITICAL(&led->lock);
    gpio_set_level(led->gpio, on ? 1 : 0);
}

esp_err_t esp32_led_output_init(esp32_led_output_t *led, int gpio, const rof_clock_t *clk,
                                uint32_t period_ms, uint32_t hold_ms)
{
    led->gpio = gpio;
    led->clk = clk;
    portMUX_INITIALIZE(&led->lock);
    led->timer = NULL;
    led_pwm_init(&led->pwm, period_ms, hold_ms);

    /* Erst den Pegel 0 vorgeben, dann den Pin als Ausgang schalten.
     * So ist er vom ersten Moment an aus (SAF-10). */
    esp_err_t err = gpio_set_level(gpio, 0);
    if (err != ESP_OK) {
        return err;
    }
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = on_tick,
        .arg = led,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "led_pwm",
    };
    err = esp_timer_create(&timer_args, &led->timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(led->timer, (uint64_t)ESP32_LED_TICK_MS * 1000u);
}

static rof_status_t led_set_duty(void *ctx, float duty)
{
    esp32_led_output_t *led = ctx;
    const uint64_t now_ms = rof_clock_now_ms(led->clk);
    portENTER_CRITICAL(&led->lock);
    const rof_status_t status = led_pwm_set_duty(&led->pwm, duty, now_ms);
    portEXIT_CRITICAL(&led->lock);
    return status;
}

rof_output_t esp32_led_output_interface(esp32_led_output_t *led)
{
    return (rof_output_t){ .ctx = led, .set_duty = led_set_duty };
}
