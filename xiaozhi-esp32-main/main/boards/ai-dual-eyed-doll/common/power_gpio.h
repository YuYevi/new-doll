#pragma once

#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_err.h>

// 板级引脚状态交接：保持原引脚状态时，先配置输出锁存值、GPIO 矩阵和方向，
// 再解除保持。禁止先复位引脚或解除保持：
// EN1 低电平有效，新使用的 GPIO48 仅支持数字 GPIO。
namespace DollGpio {
inline void Configure(gpio_num_t pin, gpio_mode_t mode) {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = mode;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    // gpio_config 选择数字 GPIO 功能，并断开外设输出。
    ESP_ERROR_CHECK(gpio_config(&config));
}

inline void ReleaseHold(gpio_num_t pin) {
    if (rtc_gpio_is_valid_gpio(pin)) {
        ESP_ERROR_CHECK(rtc_gpio_hold_dis(pin));
    } else {
        ESP_ERROR_CHECK(gpio_hold_dis(pin));
    }
}

inline void RestoreOutput(gpio_num_t pin, int level) {
    ESP_ERROR_CHECK(gpio_set_level(pin, level));
    Configure(pin, GPIO_MODE_OUTPUT);
    ReleaseHold(pin);
}

inline void RestoreInput(gpio_num_t pin) {
    Configure(pin, GPIO_MODE_INPUT);
    ReleaseHold(pin);
}

inline void Hold(gpio_num_t pin, bool output, int level = 0) {
    // 先断开 SPI/I2S/UART/PWM，建立安全的数字引脚状态。
    if (output) ESP_ERROR_CHECK(gpio_set_level(pin, level));
    Configure(pin, output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
    if (rtc_gpio_is_valid_gpio(pin)) {
        // 切换引脚复用前，先预设 RTC 输出锁存值和方向。
        if (output) ESP_ERROR_CHECK(rtc_gpio_set_level(pin, level));
        ESP_ERROR_CHECK(rtc_gpio_set_direction(
            pin, output ? RTC_GPIO_MODE_OUTPUT_ONLY : RTC_GPIO_MODE_INPUT_ONLY));
        ESP_ERROR_CHECK(rtc_gpio_pullup_dis(pin));
        ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(pin));
        ESP_ERROR_CHECK(rtc_gpio_init(pin));
        ESP_ERROR_CHECK(rtc_gpio_hold_en(pin));
    } else {
        ESP_ERROR_CHECK(gpio_hold_en(pin));
    }
}
} // DollGpio 命名空间
