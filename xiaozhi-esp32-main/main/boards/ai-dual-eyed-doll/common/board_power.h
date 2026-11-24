#pragma once

#include "config.h"
#include "power_gpio.h"

namespace DollPower {
struct Output { gpio_num_t pin; int off_level; };
inline constexpr Output kControls[] = {
    {AUDIO_PA_ENABLE_GPIO, !AUDIO_PA_ACTIVE_LEVEL},
    {DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OFF_LEVEL},
    {POWER_EN1_GPIO, !POWER_EN1_ACTIVE_LEVEL},
    {POWER_EN2_GPIO, !POWER_EN2_ACTIVE_LEVEL},
    {POWER_EN3_GPIO, !POWER_EN3_ACTIVE_LEVEL},
    {RUN_LED_GPIO, 0},
};
// 共享 I2C 上拉仍接 MCU 3.3V：将两条线保持为低电平，避免向断电的
// 编解码器反向供电。4G 关闭时，UART TX 也必须保持低电平。
inline constexpr gpio_num_t kSignalOutputs[] = {
    DISPLAY_SPI_SCLK_GPIO, DISPLAY_SPI_MOSI_GPIO, DISPLAY_SPI_DC_GPIO,
    DISPLAY_SPI_RST_GPIO, DISPLAY_1_SPI_CS_GPIO, DISPLAY_2_SPI_CS_GPIO,
    AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
    AUDIO_I2S_GPIO_DOUT, I2C_SDA_IO, I2C_SCL_IO, ML307_TX_PIN,
};
inline constexpr gpio_num_t kSignalInputs[] = {
    ML307_RX_PIN, AUDIO_I2S_GPIO_DIN, TOUCH_1_GPIO, TOUCH_2_GPIO,
    FULL_STATUS_GPIO, IMU_INT1_GPIO,
};

inline void RestoreSafeState() {
    for (const auto& control : kControls)
        DollGpio::RestoreOutput(control.pin, control.off_level);
    for (auto pin : kSignalOutputs) DollGpio::RestoreOutput(pin, 0);
    for (auto pin : kSignalInputs) DollGpio::RestoreInput(pin);
    DollGpio::RestoreInput(PWR_BUTTON_GPIO);
    // 解除全局保持前，所有引脚均已完成安全配置。
    gpio_deep_sleep_hold_dis();
}

inline void EnablePeripherals(bool cellular) {
    ESP_ERROR_CHECK(gpio_set_level(POWER_EN1_GPIO, POWER_EN1_ACTIVE_LEVEL));
    ESP_ERROR_CHECK(gpio_set_level(POWER_EN2_GPIO, POWER_EN2_ACTIVE_LEVEL));
    ESP_ERROR_CHECK(gpio_set_level(POWER_EN3_GPIO,
                                  cellular ? POWER_EN3_ACTIVE_LEVEL : !POWER_EN3_ACTIVE_LEVEL));
    ESP_ERROR_CHECK(gpio_set_level(RUN_LED_GPIO, 1));
}

inline void HoldOffState() {
    for (auto pin : kSignalOutputs) DollGpio::Hold(pin, true, 0);
    for (auto pin : kSignalInputs) DollGpio::Hold(pin, false);
    // 关闭任何一路电源前，先隔离信号引脚。
    for (const auto& control : kControls) DollGpio::Hold(control.pin, true, control.off_level);
    gpio_deep_sleep_hold_en();
}
} // DollPower 命名空间
