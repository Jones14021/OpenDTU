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
    enum class NpbInitState : uint8_t {
        Disabled = 0,
        SetEepromLock,
        RequestValidation,
        Ready,
        Fault
    };

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
    void handleNpb450Frame(const CanFrame& frame);
    void runNpb450StateMachine();
    void publishNpb450State();

    bool sendNpb450Command(uint16_t command, uint16_t data);
    bool requestNpb450Register(uint16_t command);
    bool setNpb450EepromLock();
    bool applyNpb450Setpoints();
    float getTargetCurrentFromPower();

    static bool parseBoolPayload(const String& payload, bool& out);
    static bool parseFloatPayload(const String& payload, float& out);

    uint32_t getNpb450ControllerId() const;
    uint32_t getNpb450ChargerToControllerId() const;

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

    NpbInitState _npbInitState = NpbInitState::Disabled;
    uint32_t _nextInitActionMs = 0;
    uint32_t _nextSetpointActionMs = 0;
    uint32_t _nextPollActionMs = 0;
    uint32_t _nextStatePublishMs = 0;
    uint32_t _npbInitStartMs = 0;

    uint8_t _npbAddress = 0;
    bool _npbControlEnabled = false;
    float _npbTargetWatts = 0.0f;
    float _npbChargeVoltage = 14.4f;
    float _npbMaxCurrent = 30.0f;
    float _npbMeasuredCurrent = 0.0f;
    bool _npbPsuModeOk = false;
    bool _npbEepromLockOk = false;
    bool _npbValidationSeen = false;
    bool _npbCommissioningAllowed = false;
};

extern MeanwellCanClass MeanwellCan;
