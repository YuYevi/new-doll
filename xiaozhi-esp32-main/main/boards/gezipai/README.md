# Gezipai ESP32-S3-AI-2

This board support targets the Gezipai ESP32-S3-AI-2 module with its reference baseboard.

## Hardware mapping

- Audio: ES8311 playback and ES7210 capture over I2C, using the supplied I2S pin mapping.
- Display: 240x240 ST7789 over SPI, mode 3; backlight uses GPIO4 PWM.
- Buttons: GPIO1 for push-to-talk and Wi-Fi setup, GPIO18/2 for volume.
- Status LED: GPIO17.

Battery ADC and charger-status reporting from the legacy sample are intentionally not enabled because the supplied implementation's voltage conversion and status mapping need hardware verification.

## Build

From the firmware directory, run:

```sh
python scripts/build.py gezipai --name gezipai
```

Select `Gezipai ESP32-S3-AI-2 (格子派)` in `idf.py menuconfig` under Target Board when configuring manually.
