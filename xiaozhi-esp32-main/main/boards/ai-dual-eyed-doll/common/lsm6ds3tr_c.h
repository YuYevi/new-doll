#pragma once
#include <cstddef>
#include <cstdint>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class Lsm6ds3trC {
public:
    using ShakeCallback = bool (*)(void*);
    Lsm6ds3trC(i2c_master_bus_handle_t bus, ShakeCallback callback, void* context);
    ~Lsm6ds3trC();
    bool Start();
    void Stop();
private:
    static void TaskEntry(void* arg);
    void Run();
    esp_err_t Read(uint8_t reg, uint8_t* data, size_t size);
    esp_err_t Write(uint8_t reg, uint8_t value);
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t device_ = nullptr;
    ShakeCallback callback_ = nullptr;
    void* context_ = nullptr;
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t stopped_ = nullptr;
};
