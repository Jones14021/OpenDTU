// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/gpio_types.h>

class MeanwellCanClass {
public:
    void init();
    bool isEnabled() const;

private:
    struct CanFrame {
        uint32_t id = 0;
        bool isExtended = false;
        bool isRemoteRequest = false;
        uint8_t dlc = 0;
        uint8_t data[8] = { 0 };
    };

    static void taskEntry(void* param);
    void taskLoop();

    bool initializeController();
    bool setConfigMode();
    bool setNormalMode();
    bool readFrame(CanFrame& frame);
    bool sendFrame(const CanFrame& frame);
    void handleFrame(const CanFrame& frame);
    void decodeMeanwellPbn(const CanFrame& frame);
    void publishMetric(const String& topic, float value, uint8_t decimals = 2);

    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);
    void readRegisters(uint8_t address, uint8_t* data, size_t len);
    void writeRegisters(uint8_t address, const uint8_t* data, size_t len);
    void bitModify(uint8_t address, uint8_t mask, uint8_t data);
    uint8_t readStatus();
    void resetController();

    bool _enabled = false;
    gpio_num_t _pinSck = GPIO_NUM_NC;
    gpio_num_t _pinMosi = GPIO_NUM_NC;
    gpio_num_t _pinMiso = GPIO_NUM_NC;
    gpio_num_t _pinCs = GPIO_NUM_NC;
    gpio_num_t _pinInt = GPIO_NUM_NC;
    TaskHandle_t _taskHandle = nullptr;
};

extern MeanwellCanClass MeanwellCan;
