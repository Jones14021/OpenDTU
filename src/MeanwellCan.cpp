// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "MeanwellCan.h"
#include "MqttSettings.h"
#include "PinMapping.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <algorithm>
#include <esp_log.h>
#include <inttypes.h>

#undef TAG
static const char* TAG = "meanwell_can";

namespace {
constexpr uint8_t MCP_RESET = 0xC0;
constexpr uint8_t MCP_READ = 0x03;
constexpr uint8_t MCP_WRITE = 0x02;
constexpr uint8_t MCP_BIT_MODIFY = 0x05;
constexpr uint8_t MCP_READ_STATUS = 0xA0;
constexpr uint8_t MCP_RTS_TXB0 = 0x81;

constexpr uint8_t REG_CANSTAT = 0x0E;
constexpr uint8_t REG_CANCTRL = 0x0F;
constexpr uint8_t REG_CNF3 = 0x28;
constexpr uint8_t REG_CNF2 = 0x29;
constexpr uint8_t REG_CNF1 = 0x2A;
constexpr uint8_t REG_CANINTE = 0x2B;
constexpr uint8_t REG_CANINTF = 0x2C;
constexpr uint8_t REG_RXB0CTRL = 0x60;
constexpr uint8_t REG_RXB0SIDH = 0x61;
constexpr uint8_t REG_RXB1CTRL = 0x70;
constexpr uint8_t REG_RXB1SIDH = 0x71;
constexpr uint8_t REG_TXB0CTRL = 0x30;
constexpr uint8_t REG_TXB0SIDH = 0x31;

constexpr uint8_t CANINTF_RX0IF = 0x01;
constexpr uint8_t CANINTF_RX1IF = 0x02;
constexpr uint8_t CANCTRL_MODE_MASK = 0xE0;
constexpr uint8_t CANCTRL_MODE_NORMAL = 0x00;
constexpr uint8_t CANCTRL_MODE_CONFIG = 0x80;

#ifndef MEANWELL_CAN_CNF1
#define MEANWELL_CAN_CNF1 0x01
#endif

#ifndef MEANWELL_CAN_CNF2
#define MEANWELL_CAN_CNF2 0xF0
#endif

#ifndef MEANWELL_CAN_CNF3
#define MEANWELL_CAN_CNF3 0x86
#endif

constexpr uint8_t MAX_FRAMES_PER_CYCLE = 16;
constexpr uint32_t RX_IDLE_DELAY_MS = 10;
constexpr uint32_t RX_ACTIVE_DELAY_MS = 1;

SPIClass CanSpi(VSPI);

uint16_t readU16Be(const uint8_t* data)
{
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

int16_t readS16Be(const uint8_t* data)
{
    return static_cast<int16_t>(readU16Be(data));
}
} // namespace

MeanwellCanClass MeanwellCan;

void MeanwellCanClass::init()
{
    if (!PinMapping.isValidCanConfig()) {
        ESP_LOGI(TAG, "CAN pin mapping not configured, service disabled");
        return;
    }

    auto& pinMapping = PinMapping.get();
    _pinSck = pinMapping.can_sck;
    _pinMosi = pinMapping.can_mosi;
    _pinMiso = pinMapping.can_miso;
    _pinCs = pinMapping.can_cs;
    _pinInt = pinMapping.can_int;

    pinMode(_pinCs, OUTPUT);
    digitalWrite(_pinCs, HIGH);
    if (_pinInt > GPIO_NUM_NC) {
        pinMode(_pinInt, INPUT_PULLUP);
    }

    CanSpi.begin(_pinSck, _pinMiso, _pinMosi, _pinCs);
    if (!initializeController()) {
        ESP_LOGE(TAG, "Failed to initialize MCP2515");
        return;
    }

    const String commandTopic = MqttSettings.getPrefix() + "meanwell/can/tx";
    MqttSettings.subscribe(commandTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        JsonDocument doc;
        if (deserializeJson(doc, payload, len) != DeserializationError::Ok) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/can/tx payload");
            return;
        }

        CanFrame frame;
        frame.id = doc["id"] | 0U;
        frame.isExtended = doc["ext"] | false;
        frame.isRemoteRequest = doc["rtr"] | false;

        JsonArray data = doc["data"].as<JsonArray>();
        frame.dlc = std::min<size_t>(8, data.size());
        for (uint8_t i = 0; i < frame.dlc; i++) {
            frame.data[i] = data[i] | 0;
        }

        if (doc["dlc"].is<uint8_t>()) {
            frame.dlc = std::min<uint8_t>(8, doc["dlc"].as<uint8_t>());
        }

        if (!sendFrame(frame)) {
            ESP_LOGW(TAG, "Failed to send CAN frame id=0x%08" PRIx32, frame.id);
        }
    });

    if (xTaskCreatePinnedToCore(taskEntry, "MeanwellCAN", 4096, this, 1, &_taskHandle, tskNO_AFFINITY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN task");
        return;
    }

    _enabled = true;
    ESP_LOGI(TAG, "Meanwell CAN service started");
}

bool MeanwellCanClass::isEnabled() const
{
    return _enabled;
}

void MeanwellCanClass::taskEntry(void* param)
{
    static_cast<MeanwellCanClass*>(param)->taskLoop();
}

void MeanwellCanClass::taskLoop()
{
    for (;;) {
        uint8_t processed = 0;
        while (processed < MAX_FRAMES_PER_CYCLE) {
            CanFrame frame;
            if (!readFrame(frame)) {
                break;
            }
            handleFrame(frame);
            processed++;
        }

        if (processed == 0) {
            vTaskDelay(pdMS_TO_TICKS(RX_IDLE_DELAY_MS));
        } else {
            vTaskDelay(pdMS_TO_TICKS(RX_ACTIVE_DELAY_MS));
        }
    }
}

bool MeanwellCanClass::initializeController()
{
    resetController();
    delay(10);

    if (!setConfigMode()) {
        return false;
    }

    writeRegister(REG_CNF1, MEANWELL_CAN_CNF1);
    writeRegister(REG_CNF2, MEANWELL_CAN_CNF2);
    writeRegister(REG_CNF3, MEANWELL_CAN_CNF3);

    writeRegister(REG_RXB0CTRL, 0x64);
    writeRegister(REG_RXB1CTRL, 0x60);

    writeRegister(REG_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF);
    writeRegister(REG_CANINTF, 0x00);

    return setNormalMode();
}

bool MeanwellCanClass::setConfigMode()
{
    bitModify(REG_CANCTRL, CANCTRL_MODE_MASK, CANCTRL_MODE_CONFIG);
    for (uint8_t i = 0; i < 20; i++) {
        if ((readRegister(REG_CANSTAT) & CANCTRL_MODE_MASK) == CANCTRL_MODE_CONFIG) {
            return true;
        }
        delay(2);
    }
    return false;
}

bool MeanwellCanClass::setNormalMode()
{
    bitModify(REG_CANCTRL, CANCTRL_MODE_MASK, CANCTRL_MODE_NORMAL);
    for (uint8_t i = 0; i < 20; i++) {
        if ((readRegister(REG_CANSTAT) & CANCTRL_MODE_MASK) == CANCTRL_MODE_NORMAL) {
            return true;
        }
        delay(2);
    }
    return false;
}

bool MeanwellCanClass::readFrame(CanFrame& frame)
{
    const uint8_t canIntf = readRegister(REG_CANINTF);
    uint8_t reg = 0;
    uint8_t clearMask = 0;
    if ((canIntf & CANINTF_RX0IF) != 0) {
        reg = REG_RXB0SIDH;
        clearMask = CANINTF_RX0IF;
    } else if ((canIntf & CANINTF_RX1IF) != 0) {
        reg = REG_RXB1SIDH;
        clearMask = CANINTF_RX1IF;
    } else {
        return false;
    }

    uint8_t header[13] = { 0 };
    readRegisters(reg, header, sizeof(header));

    const uint8_t sidh = header[0];
    const uint8_t sidl = header[1];
    const uint8_t eid8 = header[2];
    const uint8_t eid0 = header[3];
    const uint8_t dlc = header[4] & 0x0F;

    frame.isExtended = (sidl & 0x08) != 0;
    frame.isRemoteRequest = (header[4] & 0x40) != 0;
    frame.dlc = std::min<uint8_t>(dlc, 8);

    if (frame.isExtended) {
        frame.id = (static_cast<uint32_t>(sidh) << 21)
            | (static_cast<uint32_t>(sidl & 0xE0) << 13)
            | (static_cast<uint32_t>(sidl & 0x03) << 16)
            | (static_cast<uint32_t>(eid8) << 8)
            | eid0;
    } else {
        frame.id = (static_cast<uint32_t>(sidh) << 3) | (sidl >> 5);
    }

    memset(frame.data, 0, sizeof(frame.data));
    memcpy(frame.data, &header[5], frame.dlc);

    bitModify(REG_CANINTF, clearMask, 0x00);
    return true;
}

bool MeanwellCanClass::sendFrame(const CanFrame& frame)
{
    const uint8_t txState = readRegister(REG_TXB0CTRL) & 0x08;
    if (txState != 0) {
        return false;
    }

    uint8_t header[13] = { 0 };

    if (frame.isExtended) {
        header[0] = static_cast<uint8_t>((frame.id >> 21) & 0xFF);
        header[1] = static_cast<uint8_t>(((frame.id >> 13) & 0xE0) | 0x08 | ((frame.id >> 16) & 0x03));
        header[2] = static_cast<uint8_t>((frame.id >> 8) & 0xFF);
        header[3] = static_cast<uint8_t>(frame.id & 0xFF);
    } else {
        header[0] = static_cast<uint8_t>((frame.id >> 3) & 0xFF);
        header[1] = static_cast<uint8_t>((frame.id & 0x07) << 5);
    }

    header[4] = std::min<uint8_t>(8, frame.dlc);
    if (frame.isRemoteRequest) {
        header[4] |= 0x40;
    } else {
        memcpy(&header[5], frame.data, std::min<uint8_t>(8, frame.dlc));
    }

    writeRegisters(REG_TXB0SIDH, header, sizeof(header));

    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_RTS_TXB0);
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();

    return true;
}

void MeanwellCanClass::handleFrame(const CanFrame& frame)
{
    if (!MqttSettings.getConnected()) {
        return;
    }

    JsonDocument doc;
    doc["id"] = frame.id;
    doc["ext"] = frame.isExtended;
    doc["rtr"] = frame.isRemoteRequest;
    doc["dlc"] = frame.dlc;

    JsonArray data = doc["data"].to<JsonArray>();
    for (uint8_t i = 0; i < frame.dlc; i++) {
        data.add(frame.data[i]);
    }

    String payload;
    serializeJson(doc, payload);
    MqttSettings.publish("meanwell/can/rx", payload);

    decodeMeanwellPbn(frame);
}

void MeanwellCanClass::decodeMeanwellPbn(const CanFrame& frame)
{
    if (frame.isExtended || frame.dlc < 4) {
        return;
    }

    switch (frame.id) {
    case 0x305: {
        const float outputVoltage = readU16Be(&frame.data[0]) / 10.0f;
        const float outputCurrent = readU16Be(&frame.data[2]) / 10.0f;
        publishMetric("meanwell/charger/output_voltage", outputVoltage, 1);
        publishMetric("meanwell/charger/output_current", outputCurrent, 1);
        publishMetric("meanwell/charger/output_power", outputVoltage * outputCurrent, 1);
        break;
    }
    case 0x306: {
        const float batteryVoltage = readU16Be(&frame.data[0]) / 10.0f;
        const float batteryCurrent = readS16Be(&frame.data[2]) / 10.0f;
        publishMetric("meanwell/battery/voltage", batteryVoltage, 1);
        publishMetric("meanwell/battery/current", batteryCurrent, 1);
        publishMetric("meanwell/battery/power", batteryVoltage * batteryCurrent, 1);
        break;
    }
    case 0x307: {
        const float chargerTemp = readS16Be(&frame.data[0]) / 10.0f;
        const uint16_t stateWord = readU16Be(&frame.data[2]);
        publishMetric("meanwell/charger/temperature", chargerTemp, 1);
        MqttSettings.publish("meanwell/charger/state_word", String(stateWord));
        break;
    }
    case 0x30A: {
        const uint16_t alarmWord = readU16Be(&frame.data[0]);
        MqttSettings.publish("meanwell/charger/alarm_word", String(alarmWord));
        break;
    }
    default:
        break;
    }
}

void MeanwellCanClass::publishMetric(const String& topic, float value, const uint8_t decimals)
{
    MqttSettings.publish(topic, String(value, decimals));
}

uint8_t MeanwellCanClass::readRegister(uint8_t address)
{
    uint8_t value = 0;
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_READ);
    CanSpi.transfer(address);
    value = CanSpi.transfer(0x00);
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
    return value;
}

void MeanwellCanClass::writeRegister(uint8_t address, uint8_t value)
{
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_WRITE);
    CanSpi.transfer(address);
    CanSpi.transfer(value);
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
}

void MeanwellCanClass::readRegisters(uint8_t address, uint8_t* data, size_t len)
{
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_READ);
    CanSpi.transfer(address);
    for (size_t i = 0; i < len; i++) {
        data[i] = CanSpi.transfer(0x00);
    }
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
}

void MeanwellCanClass::writeRegisters(uint8_t address, const uint8_t* data, size_t len)
{
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_WRITE);
    CanSpi.transfer(address);
    for (size_t i = 0; i < len; i++) {
        CanSpi.transfer(data[i]);
    }
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
}

void MeanwellCanClass::bitModify(uint8_t address, uint8_t mask, uint8_t data)
{
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_BIT_MODIFY);
    CanSpi.transfer(address);
    CanSpi.transfer(mask);
    CanSpi.transfer(data);
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
}

uint8_t MeanwellCanClass::readStatus()
{
    uint8_t value = 0;
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_READ_STATUS);
    value = CanSpi.transfer(0x00);
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
    return value;
}

void MeanwellCanClass::resetController()
{
    CanSpi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pinCs, LOW);
    CanSpi.transfer(MCP_RESET);
    digitalWrite(_pinCs, HIGH);
    CanSpi.endTransaction();
}
