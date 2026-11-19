# 双眼娃娃 ESP32-S3-AI-2

本目录提供 ESP32-S3-AI-2 模组及其参考底板的板级支持，适用于双眼娃娃板卡。

## 硬件接口

- 音频：ES8311 音频播放与 ES7210 音频采集，通过 I2C 配置，I2S 引脚按参考底板定义。
- 显示：240×240 ST7789 屏幕，使用 SPI 模式 3；GPIO4 通过 PWM 控制背光。
- 按键：GPIO1 用于按住说话和进入 Wi-Fi 配网；GPIO18、GPIO2 分别用于音量增减。
- 状态灯：GPIO17。

暂未启用电池电压及充电状态读取。参考示例中的电压换算和状态映射尚未经过实板验证，因此不纳入当前实现。

## 构建

在固件仓库目录执行：

```sh
python scripts/build.py ai-dual-eyed-doll --name ai-dual-eyed-doll
```

手动配置时，在 `idf.py menuconfig` 的目标板卡菜单中选择“AI ESP32-S3-AI-2（双眼娃娃）”。
