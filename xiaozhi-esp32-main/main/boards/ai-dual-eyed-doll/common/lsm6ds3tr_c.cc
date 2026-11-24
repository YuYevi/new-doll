#include "lsm6ds3tr_c.h"
#include "config.h"
#include <esp_log.h>
#include <esp_timer.h>

namespace {
constexpr char kTag[] = "DollImu";
constexpr uint8_t kWhoAmI = 0x0f;
constexpr uint8_t kCtrl1Xl = 0x10;
constexpr uint8_t kCtrl3C = 0x12;
constexpr uint8_t kStatusReg = 0x1e;
constexpr uint8_t kOutXLXl = 0x28;
constexpr uint8_t kChipId = 0x6a;
constexpr int kTimeoutMs = 100;
constexpr int kCountsPerG = 16384; // +/-2 g full scale.
struct Vector {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};
int64_t Dot(const Vector& a, const Vector& b) {
    return static_cast<int64_t>(a.x) * b.x + static_cast<int64_t>(a.y) * b.y +
           static_cast<int64_t>(a.z) * b.z;
}
}

Lsm6ds3trC::Lsm6ds3trC(i2c_master_bus_handle_t bus, ShakeCallback callback, void* context)
    : bus_(bus), callback_(callback), context_(context) {
    stopped_ = xSemaphoreCreateBinary();
}
Lsm6ds3trC::~Lsm6ds3trC() {
    Stop();
    if (device_) i2c_master_bus_rm_device(device_);
    if (stopped_) vSemaphoreDelete(stopped_);
}
esp_err_t Lsm6ds3trC::Read(uint8_t reg, uint8_t* data, size_t size) {
    return i2c_master_transmit_receive(device_, &reg, 1, data, size, kTimeoutMs);
}
esp_err_t Lsm6ds3trC::Write(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device_, data, sizeof(data), kTimeoutMs);
}
bool Lsm6ds3trC::Start() {
    if (task_) return true;
    if (!bus_ || !callback_ || !stopped_) return false;
    if (!device_) {
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address = IMU_I2C_ADDRESS;
        cfg.scl_speed_hz = 100000;
        const auto err = i2c_master_bus_add_device(bus_, &cfg, &device_);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Add IMU failed: %s", esp_err_to_name(err));
            return false;
        }
    }
    uint8_t id = 0;
    const auto read_err = Read(kWhoAmI, &id, 1);
    if (read_err != ESP_OK || id != kChipId) {
        ESP_LOGW(kTag, "WHO_AM_I at 0x%02x: err=%s id=0x%02x (expected 0x%02x)",
                 IMU_I2C_ADDRESS, esp_err_to_name(read_err), id, kChipId);
        return false;
    }
    // BDU and register increment; 52 Hz, +/-2 g accelerometer. Gyro remains off.
    const auto ctrl_err = Write(kCtrl3C, 0x44);
    const auto accel_err = ctrl_err == ESP_OK ? Write(kCtrl1Xl, 0x30) : ctrl_err;
    if (accel_err != ESP_OK) {
        ESP_LOGW(kTag, "IMU configuration failed: %s", esp_err_to_name(accel_err));
        return false;
    }
    xSemaphoreTake(stopped_, 0);
    if (xTaskCreate(TaskEntry, "doll_imu", 3072, this, 3, &task_) != pdPASS) {
        task_ = nullptr;
        Write(kCtrl1Xl, 0x00);
        ESP_LOGW(kTag, "IMU task creation failed");
        return false;
    }
    ESP_LOGI(kTag, "LSM6DS3TR-C ready at 0x%02x, id=0x%02x", IMU_I2C_ADDRESS, id);
    return true;
}
void Lsm6ds3trC::Stop() {
    if (!task_) return;
    xTaskNotifyGive(task_);
    xSemaphoreTake(stopped_, portMAX_DELAY);
    task_ = nullptr;
    const auto err = Write(kCtrl1Xl, 0x00);
    if (err != ESP_OK) ESP_LOGW(kTag, "IMU power down failed: %s", esp_err_to_name(err));
}
void Lsm6ds3trC::TaskEntry(void* arg) {
    static_cast<Lsm6ds3trC*>(arg)->Run();
}
void Lsm6ds3trC::Run() {
    Vector gravity = {}, peak = {};
    bool initialized = false;
    int alternating_peaks = 0;
    int failures = 0;
    bool has_triggered = false;
    int64_t last_peak = 0, last_trigger = 0;
    const int64_t threshold = static_cast<int64_t>(IMU_SHAKE_THRESHOLD_MG) * kCountsPerG / 1000;
    while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(IMU_SAMPLE_MS)) == 0) {
        uint8_t status = 0, bytes[6] = {};
        esp_err_t err = Read(kStatusReg, &status, 1);
        if (err == ESP_OK && (status & 0x01) == 0) continue;
        if (err == ESP_OK) err = Read(kOutXLXl, bytes, sizeof(bytes));
        if (err != ESP_OK) {
            if (++failures == 1 || failures % 50 == 0)
                ESP_LOGW(kTag, "IMU sample failed (%d): %s", failures, esp_err_to_name(err));
            initialized = false;
            alternating_peaks = 0;
            continue;
        }
        failures = 0;
        Vector sample = {
            static_cast<int16_t>(bytes[0] | (bytes[1] << 8)),
            static_cast<int16_t>(bytes[2] | (bytes[3] << 8)),
            static_cast<int16_t>(bytes[4] | (bytes[5] << 8)),
        };
        if (!initialized) {
            gravity = sample;
            initialized = true;
            continue;
        }
        gravity.x += (sample.x - gravity.x) / 8;
        gravity.y += (sample.y - gravity.y) / 8;
        gravity.z += (sample.z - gravity.z) / 8;
        Vector motion = {sample.x - gravity.x, sample.y - gravity.y, sample.z - gravity.z};
        const int64_t now = esp_timer_get_time() / 1000;
        if (alternating_peaks && now - last_peak > IMU_PEAK_WINDOW_MS) alternating_peaks = 0;
        if (Dot(motion, motion) < threshold * threshold ||
            (alternating_peaks && now - last_peak < IMU_PEAK_SPACING_MS)) continue;
        if (alternating_peaks && Dot(motion, peak) < -(threshold * threshold / 4)) {
            ++alternating_peaks;
        } else if (!alternating_peaks || Dot(motion, peak) > 0) {
            alternating_peaks = 1;
        }
        peak = motion;
        last_peak = now;
        if (alternating_peaks >= IMU_SHAKE_PEAKS) {
            alternating_peaks = 0;
            if ((!has_triggered || now - last_trigger >= IMU_COOLDOWN_MS) && callback_(context_)) {
                has_triggered = true;
                last_trigger = now;
                ESP_LOGI(kTag, "Shake detected");
            }
        }
    }
    xSemaphoreGive(stopped_);
    vTaskDelete(nullptr);
}
