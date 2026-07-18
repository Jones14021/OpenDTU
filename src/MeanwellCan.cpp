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
#include <cmath>
#include <cstdlib>
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

constexpr uint16_t NPB_CMD_OPERATION = 0x0000;
constexpr uint16_t NPB_CMD_VOUT_SET = 0x0020;
constexpr uint16_t NPB_CMD_IOUT_SET = 0x0030;
constexpr uint16_t NPB_CMD_READ_IOUT = 0x0061;
constexpr uint16_t NPB_CMD_CURVE_CONFIG = 0x00B4;
constexpr uint16_t NPB_CMD_SYSTEM_STATUS = 0x00C1;
constexpr uint16_t NPB_CMD_SYSTEM_CONFIG = 0x00C2;

constexpr uint16_t NPB_DATA_OPERATION_ON = 0x0001;
constexpr uint16_t NPB_DATA_OPERATION_OFF = 0x0000;
constexpr uint16_t NPB_DATA_CURVE_CONFIG_PSU = 0x0004;
constexpr uint16_t NPB_DATA_SYSTEM_CONFIG_EEPOFF = 0x0400;

constexpr uint32_t NPB_BASE_CONTROLLER_TO_CHARGER = 0x000C0100;
constexpr uint32_t NPB_BASE_CHARGER_TO_CONTROLLER = 0x000C0000;

constexpr uint32_t NPB_INIT_RETRY_MS = 1000;
constexpr uint32_t NPB_VALIDATION_RETRY_MS = 1000;
constexpr uint32_t NPB_SETPOINT_INTERVAL_MS = 1000;
constexpr uint32_t NPB_POLL_INTERVAL_MS = 1000;
constexpr uint32_t NPB_STATE_PUBLISH_INTERVAL_MS = 2000;
constexpr uint32_t NPB_INIT_TIMEOUT_MS = 30000;

SPIClass CanSpi(VSPI);

uint16_t readU16Be(const uint8_t* data)
{
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

int16_t readS16Be(const uint8_t* data)
{
    return static_cast<int16_t>(readU16Be(data));
}

String payloadToString(const uint8_t* payload, size_t len)
{
    String value;
    value.reserve(len);
    for (size_t i = 0; i < len; i++) {
        value += static_cast<char>(payload[i]);
    }
    value.trim();
    return value;
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

    const String rawCommandTopic = MqttSettings.getPrefix() + "meanwell/can/tx";
    MqttSettings.subscribe(rawCommandTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
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

    const String abstractEnableTopic = MqttSettings.getPrefix() + "meanwell/npb450/control/enable";
    MqttSettings.subscribe(abstractEnableTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        bool enabled = false;
        if (!parseBoolPayload(payloadToString(payload, len), enabled)) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/npb450/control/enable payload");
            return;
        }
        _npbControlEnabled = enabled;
    });

    const String abstractTargetWattsTopic = MqttSettings.getPrefix() + "meanwell/npb450/control/target_w";
    MqttSettings.subscribe(abstractTargetWattsTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        float watts = 0.0f;
        if (!parseFloatPayload(payloadToString(payload, len), watts)) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/npb450/control/target_w payload");
            return;
        }
        _npbTargetWatts = std::max(0.0f, watts);
    });

    const String abstractConfigVoltageTopic = MqttSettings.getPrefix() + "meanwell/npb450/config/charge_voltage_v";
    MqttSettings.subscribe(abstractConfigVoltageTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        float voltage = 0.0f;
        if (!parseFloatPayload(payloadToString(payload, len), voltage)) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/npb450/config/charge_voltage_v payload");
            return;
        }
        _npbChargeVoltage = std::clamp(voltage, 0.0f, 60.0f);
    });

    const String abstractConfigMaxCurrentTopic = MqttSettings.getPrefix() + "meanwell/npb450/config/max_current_a";
    MqttSettings.subscribe(abstractConfigMaxCurrentTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        float current = 0.0f;
        if (!parseFloatPayload(payloadToString(payload, len), current)) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/npb450/config/max_current_a payload");
            return;
        }
        _npbMaxCurrent = std::clamp(current, 0.0f, 100.0f);
    });

    const String abstractConfigAddressTopic = MqttSettings.getPrefix() + "meanwell/npb450/config/address";
    MqttSettings.subscribe(abstractConfigAddressTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        float address = 0.0f;
        if (!parseFloatPayload(payloadToString(payload, len), address)) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/npb450/config/address payload");
            return;
        }
        _npbAddress = static_cast<uint8_t>(std::clamp(address, 0.0f, 15.0f));
    });

    const String abstractCommissionTopic = MqttSettings.getPrefix() + "meanwell/npb450/control/commission_psu";
    MqttSettings.subscribe(abstractCommissionTopic, 0, [this](const espMqttClientTypes::MessageProperties&, const char*, const uint8_t* payload, size_t len) {
        bool enableCommissioning = false;
        if (!parseBoolPayload(payloadToString(payload, len), enableCommissioning)) {
            ESP_LOGW(TAG, "Ignoring invalid meanwell/npb450/control/commission_psu payload");
            return;
        }
        _npbCommissioningAllowed = enableCommissioning;
        if (_npbCommissioningAllowed) {
            if (!sendNpb450Command(NPB_CMD_CURVE_CONFIG, NPB_DATA_CURVE_CONFIG_PSU)) {
                ESP_LOGW(TAG, "Failed to send PSU commissioning command");
            }
            _npbCommissioningAllowed = false;
        }
    });

    _npbInitState = NpbInitState::SetEepromLock;
    _npbInitStartMs = millis();
    _nextInitActionMs = 0;
    _nextSetpointActionMs = 0;
    _nextPollActionMs = 0;
    _nextStatePublishMs = 0;

    if (xTaskCreatePinnedToCore(taskEntry, "MeanwellCAN", 6144, this, 1, &_taskHandle, tskNO_AFFINITY) != pdPASS) {
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

        runNpb450StateMachine();

        if (processed == 0) {
            vTaskDelay(pdMS_TO_TICKS(RX_IDLE_DELAY_MS));
        } else {
            vTaskDelay(pdMS_TO_TICKS(RX_ACTIVE_DELAY_MS));
        }
    }
}

void MeanwellCanClass::runNpb450StateMachine()
{
    const uint32_t now = millis();

    if (now >= _nextStatePublishMs) {
        publishNpb450State();
        _nextStatePublishMs = now + NPB_STATE_PUBLISH_INTERVAL_MS;
    }

    if (_npbInitState == NpbInitState::Fault || _npbInitState == NpbInitState::Disabled) {
        return;
    }

    if ((now - _npbInitStartMs) > NPB_INIT_TIMEOUT_MS && _npbInitState != NpbInitState::Ready) {
        _npbInitState = NpbInitState::Fault;
        return;
    }

    if (_npbInitState == NpbInitState::SetEepromLock) {
        if (now < _nextInitActionMs) {
            return;
        }

        if (setNpb450EepromLock()) {
            _npbInitState = NpbInitState::RequestValidation;
            _nextInitActionMs = now;
        } else {
            _nextInitActionMs = now + NPB_INIT_RETRY_MS;
        }
        return;
    }

    if (_npbInitState == NpbInitState::RequestValidation) {
        if (now >= _nextInitActionMs) {
            bool requestOk = true;
            requestOk &= requestNpb450Register(NPB_CMD_SYSTEM_STATUS);
            requestOk &= requestNpb450Register(NPB_CMD_SYSTEM_CONFIG);
            requestOk &= requestNpb450Register(NPB_CMD_READ_IOUT);

            if (!requestOk) {
                _nextInitActionMs = now + NPB_VALIDATION_RETRY_MS;
                return;
            }

            _nextInitActionMs = now + NPB_VALIDATION_RETRY_MS;
        }

        if (_npbValidationSeen && _npbPsuModeOk && _npbEepromLockOk) {
            _npbInitState = NpbInitState::Ready;
            _nextSetpointActionMs = now;
            _nextPollActionMs = now;
        }
        return;
    }

    if (_npbInitState == NpbInitState::Ready) {
        if (now >= _nextPollActionMs) {
            requestNpb450Register(NPB_CMD_SYSTEM_STATUS);
            requestNpb450Register(NPB_CMD_SYSTEM_CONFIG);
            requestNpb450Register(NPB_CMD_READ_IOUT);
            _nextPollActionMs = now + NPB_POLL_INTERVAL_MS;
        }

        if (now >= _nextSetpointActionMs) {
            applyNpb450Setpoints();
            _nextSetpointActionMs = now + NPB_SETPOINT_INTERVAL_MS;
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

bool MeanwellCanClass::sendNpb450Command(uint16_t command, uint16_t data)
{
    CanFrame frame;
    frame.id = getNpb450ControllerId();
    frame.isExtended = true;
    frame.isRemoteRequest = false;
    frame.dlc = 4;
    frame.data[0] = static_cast<uint8_t>(command & 0xFF);
    frame.data[1] = static_cast<uint8_t>((command >> 8) & 0xFF);
    frame.data[2] = static_cast<uint8_t>(data & 0xFF);
    frame.data[3] = static_cast<uint8_t>((data >> 8) & 0xFF);
    return sendFrame(frame);
}

bool MeanwellCanClass::requestNpb450Register(uint16_t command)
{
    return sendNpb450Command(command, 0x0000);
}

bool MeanwellCanClass::setNpb450EepromLock()
{
    return sendNpb450Command(NPB_CMD_SYSTEM_CONFIG, NPB_DATA_SYSTEM_CONFIG_EEPOFF);
}

bool MeanwellCanClass::applyNpb450Setpoints()
{
    if (!_npbControlEnabled || _npbTargetWatts <= 0.0f) {
        return sendNpb450Command(NPB_CMD_OPERATION, NPB_DATA_OPERATION_OFF);
    }

    const float targetCurrent = getTargetCurrentFromPower();
    const uint16_t currentScaled = static_cast<uint16_t>(std::clamp(targetCurrent * 100.0f, 0.0f, 65535.0f));
    const uint16_t voltageScaled = static_cast<uint16_t>(std::clamp(_npbChargeVoltage * 100.0f, 0.0f, 65535.0f));

    bool ok = true;
    ok &= sendNpb450Command(NPB_CMD_VOUT_SET, voltageScaled);
    ok &= sendNpb450Command(NPB_CMD_IOUT_SET, currentScaled);
    ok &= sendNpb450Command(NPB_CMD_OPERATION, NPB_DATA_OPERATION_ON);

    return ok;
}

float MeanwellCanClass::getTargetCurrentFromPower()
{
    if (_npbChargeVoltage <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(_npbTargetWatts / _npbChargeVoltage, 0.0f, _npbMaxCurrent);
}

void MeanwellCanClass::handleFrame(const CanFrame& frame)
{
    if (MqttSettings.getConnected()) {
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
    }

    handleNpb450Frame(frame);
    decodeMeanwellPbn(frame);
}

void MeanwellCanClass::handleNpb450Frame(const CanFrame& frame)
{
    if (!frame.isExtended || frame.dlc < 4) {
        return;
    }

    if (frame.id != getNpb450ChargerToControllerId()) {
        return;
    }

    const uint16_t command = static_cast<uint16_t>(frame.data[0]) | (static_cast<uint16_t>(frame.data[1]) << 8);
    const uint16_t value = static_cast<uint16_t>(frame.data[2]) | (static_cast<uint16_t>(frame.data[3]) << 8);

    switch (command) {
    case NPB_CMD_SYSTEM_STATUS:
        _npbPsuModeOk = (value & 0x8000U) == 0;
        _npbValidationSeen = true;
        if (MqttSettings.getConnected()) {
            MqttSettings.publish("meanwell/npb450/status/system_status", String(value));
            MqttSettings.publish("meanwell/npb450/status/psu_mode_ok", _npbPsuModeOk ? "1" : "0");
        }
        break;
    case NPB_CMD_SYSTEM_CONFIG:
        _npbEepromLockOk = (value & NPB_DATA_SYSTEM_CONFIG_EEPOFF) != 0;
        _npbValidationSeen = true;
        if (MqttSettings.getConnected()) {
            MqttSettings.publish("meanwell/npb450/status/system_config", String(value));
            MqttSettings.publish("meanwell/npb450/status/eeprom_lock_ok", _npbEepromLockOk ? "1" : "0");
        }
        break;
    case NPB_CMD_READ_IOUT:
        _npbMeasuredCurrent = static_cast<float>(value) / 100.0f;
        publishMetric("meanwell/npb450/status/iout_actual", _npbMeasuredCurrent, 2);
        break;
    default:
        break;
    }
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

void MeanwellCanClass::publishNpb450State()
{
    if (!MqttSettings.getConnected()) {
        return;
    }

    String initState = "disabled";
    switch (_npbInitState) {
    case NpbInitState::Disabled:
        initState = "disabled";
        break;
    case NpbInitState::SetEepromLock:
        initState = "set_eeprom_lock";
        break;
    case NpbInitState::RequestValidation:
        initState = "request_validation";
        break;
    case NpbInitState::Ready:
        initState = "ready";
        break;
    case NpbInitState::Fault:
        initState = "fault";
        break;
    }

    MqttSettings.publish("meanwell/npb450/status/init_state", initState);
    MqttSettings.publish("meanwell/npb450/status/control_enabled", _npbControlEnabled ? "1" : "0");
    publishMetric("meanwell/npb450/status/target_w", _npbTargetWatts, 1);
    publishMetric("meanwell/npb450/status/target_iout", getTargetCurrentFromPower(), 2);
    publishMetric("meanwell/npb450/status/target_vout", _npbChargeVoltage, 2);
    MqttSettings.publish("meanwell/npb450/status/psu_mode_ok", _npbPsuModeOk ? "1" : "0");
    MqttSettings.publish("meanwell/npb450/status/eeprom_lock_ok", _npbEepromLockOk ? "1" : "0");
    MqttSettings.publish("meanwell/npb450/status/address", String(_npbAddress));
}

bool MeanwellCanClass::parseBoolPayload(const String& payload, bool& out)
{
    String value = payload;
    value.trim();
    value.toLowerCase();

    if (value == "1" || value == "true" || value == "on") {
        out = true;
        return true;
    }

    if (value == "0" || value == "false" || value == "off") {
        out = false;
        return true;
    }

    return false;
}

bool MeanwellCanClass::parseFloatPayload(const String& payload, float& out)
{
    String value = payload;
    value.trim();
    if (value.isEmpty()) {
        return false;
    }

    char buffer[32] = { 0 };
    value.toCharArray(buffer, sizeof(buffer));
    char* endPtr = nullptr;
    const float parsed = std::strtof(buffer, &endPtr);
    if (endPtr == buffer || !std::isfinite(parsed)) {
        return false;
    }

    out = parsed;
    return true;
}

uint32_t MeanwellCanClass::getNpb450ControllerId() const
{
    return NPB_BASE_CONTROLLER_TO_CHARGER + _npbAddress;
}

uint32_t MeanwellCanClass::getNpb450ChargerToControllerId() const
{
    return NPB_BASE_CHARGER_TO_CONTROLLER + _npbAddress;
}

void MeanwellCanClass::publishMetric(const String& topic, float value, const uint8_t decimals)
{
    if (!MqttSettings.getConnected()) {
        return;
    }
    MqttSettings.publish(topic, String(value, static_cast<unsigned int>(decimals)));
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
