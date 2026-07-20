// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "MqttHandleHass.h"
#include "MeanwellCan.h"
#include "MqttHandleInverter.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "Utils.h"
#include "__compiled_constants.h"
#include "defaults.h"

#define MAX_CONFIG_PUBLISH_RATIO 60000

#undef TAG
static const char* TAG = "mqtt";

MqttHandleHassClass MqttHandleHass;

MqttHandleHassClass::MqttHandleHassClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleHassClass::loop, this))
{
}

void MqttHandleHassClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void MqttHandleHassClass::loop()
{
    if (MqttSettings.getConnected() && !_wasConnected) {
        // Connection established
        _wasConnected = true;
        _updateForced = true;
    } else if (!MqttSettings.getConnected() && _wasConnected) {
        // Connection lost
        _wasConnected = false;
    }

    if (_updateForced && _publishConfigTimeout.occured()) {
        publishConfig();
        _updateForced = false;
    }
}

void MqttHandleHassClass::forceUpdate()
{
    _updateForced = true;
}

void MqttHandleHassClass::publishConfig()
{
    if (!Configuration.get().Mqtt.Hass.Enabled) {
        return;
    }

    if (!MqttSettings.getConnected() && Hoymiles.isAllRadioIdle()) {
        return;
    }

    ESP_LOGI(TAG, "Publish HA config");
    _publishConfigTimeout.set(MAX_CONFIG_PUBLISH_RATIO);

    const CONFIG_T& config = Configuration.get();

    // publish DTU sensors
    publishDtuSensor("IP", "dtu/ip", "", "mdi:network-outline", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("WiFi Signal", "dtu/rssi", "dBm", "", DEVICE_CLS_SIGNAL_STRENGTH, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("Uptime", "dtu/uptime", "s", "", DEVICE_CLS_DURATION, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("Temperature", "dtu/temperature", "°C", "", DEVICE_CLS_TEMPERATURE, STATE_CLS_MEASUREMENT, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("Heap Size", "dtu/heap/size", "Bytes", "mdi:memory", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("Heap Free", "dtu/heap/free", "Bytes", "mdi:memory", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("Largest Free Heap Block", "dtu/heap/maxalloc", "Bytes", "mdi:memory", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
    publishDtuSensor("Lifetime Minimum Free Heap", "dtu/heap/minfree", "Bytes", "mdi:memory", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);

    publishDtuSensor("Yield Total", "ac/yieldtotal", "kWh", "", DEVICE_CLS_ENERGY, STATE_CLS_TOTAL_INCREASING, CATEGORY_NONE);
    publishDtuSensor("Yield Day", "ac/yieldday", "Wh", "", DEVICE_CLS_ENERGY, STATE_CLS_TOTAL_INCREASING, CATEGORY_NONE);
    publishDtuSensor("AC Power", "ac/power", "W", "", DEVICE_CLS_PWR, STATE_CLS_MEASUREMENT, CATEGORY_NONE);
    publishDtuSensor("DC Power", "dc/power", "W", "", DEVICE_CLS_PWR, STATE_CLS_MEASUREMENT, CATEGORY_NONE);

    publishDtuBinarySensor("Status", config.Mqtt.Lwt.Topic, config.Mqtt.Lwt.Value_Online, config.Mqtt.Lwt.Value_Offline, DEVICE_CLS_CONNECTIVITY, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);

    // Loop all inverters
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        yield();

        publishInverterButton(inv, "Turn Inverter Off", "cmd/power", "0", "mdi:power-plug-off", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_CONFIG);
        publishInverterButton(inv, "Turn Inverter On", "cmd/power", "1", "mdi:power-plug", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_CONFIG);
        publishInverterButton(inv, "Restart Inverter", "cmd/restart", "1", "", DEVICE_CLS_RESTART, STATE_CLS_NONE, CATEGORY_CONFIG);
        publishInverterButton(inv, "Reset Radio Statistics", "cmd/reset_rf_stats", "1", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_CONFIG);

        publishInverterNumber(inv, "Limit NonPersistent Relative", "status/limit_relative", "cmd/limit_nonpersistent_relative", 0, 100, 0.1, "%", "mdi:speedometer", STATE_CLS_NONE, CATEGORY_CONFIG);
        publishInverterNumber(inv, "Limit Persistent Relative", "status/limit_relative", "cmd/limit_persistent_relative", 0, 100, 0.1, "%", "mdi:speedometer", STATE_CLS_NONE, CATEGORY_CONFIG);

        publishInverterNumber(inv, "Limit NonPersistent Absolute", "status/limit_absolute", "cmd/limit_nonpersistent_absolute", 0, MAX_INVERTER_LIMIT, 1, "W", "mdi:speedometer", STATE_CLS_NONE, CATEGORY_CONFIG);
        publishInverterNumber(inv, "Limit Persistent Absolute", "status/limit_absolute", "cmd/limit_persistent_absolute", 0, MAX_INVERTER_LIMIT, 1, "W", "mdi:speedometer", STATE_CLS_NONE, CATEGORY_CONFIG);

        publishInverterBinarySensor(inv, "Reachable", "status/reachable", "1", "0", DEVICE_CLS_CONNECTIVITY, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterBinarySensor(inv, "Producing", "status/producing", "1", "0", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_NONE);

        publishInverterSensor(inv, "TX Requests", "radio/tx_request", "", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterSensor(inv, "RX Success", "radio/rx_success", "", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterSensor(inv, "RX Fail Receive Nothing", "radio/rx_fail_nothing", "", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterSensor(inv, "RX Fail Receive Partial", "radio/rx_fail_partial", "", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterSensor(inv, "RX Fail Receive Corrupt", "radio/rx_fail_corrupt", "", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterSensor(inv, "TX Re-Request Fragment", "radio/tx_re_request", "", "", DEVICE_CLS_NONE, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);
        publishInverterSensor(inv, "RSSI", "radio/rssi", "dBm", "", DEVICE_CLS_SIGNAL_STRENGTH, STATE_CLS_NONE, CATEGORY_DIAGNOSTIC);

        // Loop all channels
        for (auto& t : inv->Statistics()->getChannelTypes()) {
            for (auto& c : inv->Statistics()->getChannelsByType(t)) {
                for (uint8_t f = 0; f < DEVICE_CLS_ASSIGN_LIST_LEN; f++) {
                    bool clear = false;
                    if (t == TYPE_DC && !config.Mqtt.Hass.IndividualPanels) {
                        clear = true;
                    }
                    publishInverterField(inv, t, c, deviceFieldAssignment[f], clear);
                    yield();
                }
            }
        }
    }

    if (MeanwellCan.isEnabled()) {
        publishMeanwellNpb450Config();
    } else {
        clearMeanwellNpb450Config();
    }
}

void MqttHandleHassClass::publishInverterField(std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const ChannelNum_t channel, const byteAssign_fieldDeviceClass_t fieldType, const bool clear)
{
    if (!inv->Statistics()->hasChannelFieldValue(type, channel, fieldType.fieldId)) {
        return;
    }

    const String serial = inv->serialString();

    String fieldName;
    if (type == TYPE_INV && fieldType.fieldId == FLD_PDC) {
        fieldName = "PowerDC";
    } else {
        fieldName = inv->Statistics()->getChannelFieldName(type, channel, fieldType.fieldId);
    }

    String chanNum;
    if (type == TYPE_DC) {
        // TODO(tbnobody)
        chanNum = static_cast<uint8_t>(channel) + 1;
    } else {
        chanNum = channel;
    }

    const String configTopic = "sensor/dtu_" + serial
        + "/" + "ch" + chanNum + "_" + fieldName
        + "/config";

    if (!clear) {
        const String stateTopic = MqttSettings.getPrefix() + MqttHandleInverter.getTopic(inv, type, channel, fieldType.fieldId);

        String name;
        if (type != TYPE_DC) {
            name = fieldName;
        } else {
            name = "CH" + chanNum + " " + fieldName;
        }

        String unit_of_measure = inv->Statistics()->getChannelFieldUnit(type, channel, fieldType.fieldId);

        JsonDocument root;
        createInverterInfo(root, inv);
        addCommonMetadata(root, unit_of_measure, "", fieldType.deviceClsId, fieldType.stateClsId, CATEGORY_NONE);

        root["name"] = name;
        root["stat_t"] = stateTopic;
        root["uniq_id"] = serial + "_ch" + chanNum + "_" + fieldName;

        if (Configuration.get().Mqtt.Hass.Expire) {
            root["exp_aft"] = Hoymiles.getNumInverters() * max<uint32_t>(Hoymiles.PollInterval(), Configuration.get().Mqtt.PublishInterval) * inv->getReachableThreshold();
        }

        publish(configTopic, root);
    } else {
        publish(configTopic, "");
    }
}

void MqttHandleHassClass::publishInverterButton(
    std::shared_ptr<InverterAbstract> inv, const String& name, const String& state_topic, const String& payload,
    const String& icon,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    const String serial = inv->serialString();

    String buttonId = name;
    buttonId.replace(" ", "_");
    buttonId.toLowerCase();

    const String configTopic = "button/dtu_" + serial
        + "/" + buttonId
        + "/config";

    const String cmdTopic = MqttSettings.getPrefix() + serial + "/" + state_topic;

    JsonDocument root;
    createInverterInfo(root, inv);
    addCommonMetadata(root, "", icon, device_class, state_class, category);

    root["name"] = name;
    root["uniq_id"] = serial + "_" + buttonId;
    root["cmd_t"] = cmdTopic;
    root["payload_press"] = payload;

    publish(configTopic, root);
}

void MqttHandleHassClass::publishInverterNumber(
    std::shared_ptr<InverterAbstract> inv, const String& name,
    const String& stateTopic, const String& command_topic,
    const int16_t min, const int16_t max, float step,
    const String& unit_of_measure, const String& icon,
    const StateClassType state_class, const CategoryType category)
{
    const String serial = inv->serialString();

    String buttonId = name;
    buttonId.replace(" ", "_");
    buttonId.toLowerCase();

    const String configTopic = "number/dtu_" + serial
        + "/" + buttonId
        + "/config";

    const String cmdTopic = MqttSettings.getPrefix() + serial + "/" + command_topic;
    const String statTopic = MqttSettings.getPrefix() + serial + "/" + stateTopic;

    JsonDocument root;
    createInverterInfo(root, inv);
    addCommonMetadata(root, unit_of_measure, icon, DEVICE_CLS_NONE, state_class, category);

    root["name"] = name;
    root["uniq_id"] = serial + "_" + buttonId;
    root["cmd_t"] = cmdTopic;
    root["stat_t"] = statTopic;
    root["min"] = min;
    root["max"] = max;
    root["step"] = step;

    publish(configTopic, root);
}

void MqttHandleHassClass::createInverterInfo(JsonDocument& root, std::shared_ptr<InverterAbstract> inv)
{
    createDeviceInfo(
        root,
        inv->name(),
        inv->serialString(),
        getDtuUrl(),
        "OpenDTU",
        inv->typeName(),
        __COMPILED_GIT_HASH__,
        getDtuUniqueId());
}

void MqttHandleHassClass::createDtuInfo(JsonDocument& root)
{
    createDeviceInfo(
        root,
        NetworkSettings.getHostname(),
        getDtuUniqueId(),
        getDtuUrl(),
        "OpenDTU",
        "OpenDTU",
        __COMPILED_GIT_HASH__);
}

void MqttHandleHassClass::createMeanwellNpb450Info(JsonDocument& root)
{
    createDeviceInfo(
        root,
        "Meanwell NPB 450-12",
        getMeanwellNpb450UniqueId(),
        getDtuUrl(),
        "Mean Well",
        "NPB-450-12",
        __COMPILED_GIT_HASH__,
        getDtuUniqueId());
}

void MqttHandleHassClass::createDeviceInfo(
    JsonDocument& root,
    const String& name, const String& identifiers, const String& configuration_url,
    const String& manufacturer, const String& model, const String& sw_version,
    const String& via_device)
{
    auto object = root["dev"].to<JsonObject>();

    object["name"] = name;
    object["ids"] = identifiers;
    object["cu"] = configuration_url;
    object["mf"] = manufacturer;
    object["mdl"] = model;
    object["sw"] = sw_version;

    if (via_device != "") {
        object["via_device"] = via_device;
    }
}

String MqttHandleHassClass::getDtuUniqueId()
{
    return NetworkSettings.getHostname() + "_" + Utils::getChipId();
}

String MqttHandleHassClass::getDtuUrl()
{
    return String("http://") + NetworkSettings.localIP().toString();
}

String MqttHandleHassClass::getMeanwellNpb450UniqueId()
{
    return getDtuUniqueId() + "_meanwell_npb450_12";
}

String MqttHandleHassClass::getMeanwellNpb450RootDevice()
{
    return "meanwell_npb450_12_" + getDtuUniqueId();
}

void MqttHandleHassClass::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic, payload, Configuration.get().Mqtt.Hass.Retain);
    yield();
}

void MqttHandleHassClass::publish(const String& subtopic, const JsonDocument& doc)
{
    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return;
    }
    String buffer;
    serializeJson(doc, buffer);
    publish(subtopic, buffer);
}

void MqttHandleHassClass::addCommonMetadata(
    JsonDocument& doc,
    const String& unit_of_measure, const String& icon,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    if (unit_of_measure != "") {
        doc["unit_of_meas"] = unit_of_measure;
    }
    if (icon != "") {
        doc["ic"] = icon;
    }
    if (device_class != DEVICE_CLS_NONE) {
        doc["dev_cla"] = deviceClass_name[device_class];
    }
    if (state_class != STATE_CLS_NONE) {
        doc["stat_cla"] = stateClass_name[state_class];
    }
    if (category != CATEGORY_NONE) {
        doc["ent_cat"] = category_name[category];
    }
}

void MqttHandleHassClass::publishBinarySensor(
    JsonDocument& doc,
    const String& root_device, const String& unique_id_prefix, const String& name, const String& state_topic, const String& payload_on, const String& payload_off,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    String sensor_id = name;
    sensor_id.toLowerCase();
    sensor_id.replace(" ", "_");

    doc["name"] = name;
    doc["uniq_id"] = unique_id_prefix + "_" + sensor_id;
    doc["stat_t"] = MqttSettings.getPrefix() + state_topic;
    doc["pl_on"] = payload_on;
    doc["pl_off"] = payload_off;

    addCommonMetadata(doc, "", "", device_class, state_class, category);

    const String configTopic = "binary_sensor/" + root_device + "/" + sensor_id + "/config";
    publish(configTopic, doc);
}

void MqttHandleHassClass::publishDtuBinarySensor(
    const String& name, const String& state_topic, const String& payload_on, const String& payload_off,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    const String dtuId = getDtuUniqueId();

    JsonDocument root;
    createDtuInfo(root);
    publishBinarySensor(root, dtuId, dtuId, name, state_topic, payload_on, payload_off, device_class, state_class, category);
}

void MqttHandleHassClass::publishInverterBinarySensor(
    std::shared_ptr<InverterAbstract> inv, const String& name, const String& state_topic, const String& payload_on, const String& payload_off,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    const String serial = inv->serialString();

    JsonDocument root;
    createInverterInfo(root, inv);
    publishBinarySensor(root, "dtu_" + serial, serial, name, serial + "/" + state_topic, payload_on, payload_off, device_class, state_class, category);
}

void MqttHandleHassClass::publishSensor(
    JsonDocument& doc,
    const String& root_device, const String& unique_id_prefix, const String& name, const String& state_topic,
    const String& unit_of_measure, const String& icon,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    String sensor_id = name;
    sensor_id.toLowerCase();
    sensor_id.replace(" ", "_");

    doc["name"] = name;
    doc["uniq_id"] = unique_id_prefix + "_" + sensor_id;
    doc["stat_t"] = MqttSettings.getPrefix() + state_topic;

    addCommonMetadata(doc, unit_of_measure, icon, device_class, state_class, category);

    const CONFIG_T& config = Configuration.get();
    doc["avty_t"] = MqttSettings.getPrefix() + config.Mqtt.Lwt.Topic;
    doc["pl_avail"] = config.Mqtt.Lwt.Value_Online;
    doc["pl_not_avail"] = config.Mqtt.Lwt.Value_Offline;

    const String configTopic = "sensor/" + root_device + "/" + sensor_id + "/config";
    publish(configTopic, doc);
}

void MqttHandleHassClass::publishDtuSensor(
    const String& name, const String& state_topic,
    const String& unit_of_measure, const String& icon,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    const String dtuId = getDtuUniqueId();

    JsonDocument root;
    createDtuInfo(root);
    publishSensor(root, dtuId, dtuId, name, state_topic, unit_of_measure, icon, device_class, state_class, category);
}

void MqttHandleHassClass::publishInverterSensor(
    std::shared_ptr<InverterAbstract> inv, const String& name, const String& state_topic,
    const String& unit_of_measure, const String& icon,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    const String serial = inv->serialString();

    JsonDocument root;
    createInverterInfo(root, inv);
    publishSensor(root, "dtu_" + serial, serial, name, serial + "/" + state_topic, unit_of_measure, icon, device_class, state_class, category);
}

void MqttHandleHassClass::publishMeanwellNpb450Config()
{
    publishMeanwellNpb450Switch(
        "Control Enable",
        "meanwell/npb450/status/control_enabled",
        "meanwell/npb450/control/enable",
        "ON",
        "OFF",
        "1",
        "0",
        "mdi:toggle-switch",
        CATEGORY_CONFIG);

    publishMeanwellNpb450Number(
        "Target Watt",
        "meanwell/npb450/status/target_w",
        "meanwell/npb450/control/target_w",
        0.0f,
        450.0f,
        1.0f,
        "W",
        "mdi:flash",
        STATE_CLS_NONE,
        CATEGORY_CONFIG);

    publishMeanwellNpb450Number(
        "Charge Voltage",
        "meanwell/npb450/status/target_vout",
        "meanwell/npb450/config/charge_voltage_v",
        0.0f,
        60.0f,
        0.1f,
        "V",
        "mdi:flash-triangle",
        STATE_CLS_NONE,
        CATEGORY_CONFIG);

    publishMeanwellNpb450Number(
        "Max Current",
        "meanwell/npb450/status/max_current",
        "meanwell/npb450/config/max_current_a",
        0.0f,
        100.0f,
        0.1f,
        "A",
        "mdi:current-dc",
        STATE_CLS_NONE,
        CATEGORY_CONFIG);

    publishMeanwellNpb450Number(
        "Address",
        "meanwell/npb450/status/address",
        "meanwell/npb450/config/address",
        0.0f,
        15.0f,
        1.0f,
        "",
        "mdi:identifier",
        STATE_CLS_NONE,
        CATEGORY_CONFIG);

    publishMeanwellNpb450Button(
        "Commission PSU",
        "meanwell/npb450/control/commission_psu",
        "1",
        "mdi:wrench-cog",
        CATEGORY_CONFIG);

    publishMeanwellNpb450Sensor(
        "Init State",
        "meanwell/npb450/status/init_state",
        "",
        "mdi:state-machine",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450BinarySensor(
        "PSU Mode OK",
        "meanwell/npb450/status/psu_mode_ok",
        "1",
        "0",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450BinarySensor(
        "EEPROM Lock OK",
        "meanwell/npb450/status/eeprom_lock_ok",
        "1",
        "0",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450BinarySensor(
        "Validation Seen",
        "meanwell/npb450/status/validation_seen",
        "1",
        "0",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450BinarySensor(
        "Measured Current Seen",
        "meanwell/npb450/status/iout_actual_seen",
        "1",
        "0",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450BinarySensor(
        "Commissioning Pending",
        "meanwell/npb450/status/commissioning_pending",
        "1",
        "0",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "Target Current",
        "meanwell/npb450/status/target_iout",
        "A",
        "mdi:current-dc",
        DEVICE_CLS_CURRENT,
        STATE_CLS_MEASUREMENT,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "Measured Current",
        "meanwell/npb450/status/iout_actual",
        "A",
        "mdi:current-dc",
        DEVICE_CLS_CURRENT,
        STATE_CLS_MEASUREMENT,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "System Status",
        "meanwell/npb450/status/system_status",
        "",
        "mdi:information-outline",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "System Config",
        "meanwell/npb450/status/system_config",
        "",
        "mdi:cog-outline",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "Output Voltage",
        "meanwell/charger/output_voltage",
        "V",
        "",
        DEVICE_CLS_VOLTAGE,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);

    publishMeanwellNpb450Sensor(
        "Output Current",
        "meanwell/charger/output_current",
        "A",
        "",
        DEVICE_CLS_CURRENT,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);

    publishMeanwellNpb450Sensor(
        "Output Power",
        "meanwell/charger/output_power",
        "W",
        "",
        DEVICE_CLS_PWR,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);

    publishMeanwellNpb450Sensor(
        "Temperature",
        "meanwell/charger/temperature",
        "°C",
        "",
        DEVICE_CLS_TEMPERATURE,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);

    publishMeanwellNpb450Sensor(
        "State Word",
        "meanwell/charger/state_word",
        "",
        "mdi:counter",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "Alarm Word",
        "meanwell/charger/alarm_word",
        "",
        "mdi:alert-outline",
        DEVICE_CLS_NONE,
        STATE_CLS_NONE,
        CATEGORY_DIAGNOSTIC);

    publishMeanwellNpb450Sensor(
        "Battery Voltage",
        "meanwell/battery/voltage",
        "V",
        "",
        DEVICE_CLS_VOLTAGE,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);

    publishMeanwellNpb450Sensor(
        "Battery Current",
        "meanwell/battery/current",
        "A",
        "",
        DEVICE_CLS_CURRENT,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);

    publishMeanwellNpb450Sensor(
        "Battery Power",
        "meanwell/battery/power",
        "W",
        "",
        DEVICE_CLS_PWR,
        STATE_CLS_MEASUREMENT,
        CATEGORY_NONE);
}

void MqttHandleHassClass::clearMeanwellNpb450Config()
{
    ESP_LOGD(TAG, "Clearing Meanwell NPB-450 HA discovery config (service disabled)");

    const String root = getMeanwellNpb450RootDevice();

    publish("switch/" + root + "/control_enable/config", "");

    publish("number/" + root + "/target_watt/config", "");
    publish("number/" + root + "/charge_voltage/config", "");
    publish("number/" + root + "/max_current/config", "");
    publish("number/" + root + "/address/config", "");

    publish("button/" + root + "/commission_psu/config", "");

    publish("sensor/" + root + "/init_state/config", "");
    publish("binary_sensor/" + root + "/psu_mode_ok/config", "");
    publish("binary_sensor/" + root + "/eeprom_lock_ok/config", "");
    publish("binary_sensor/" + root + "/validation_seen/config", "");
    publish("binary_sensor/" + root + "/measured_current_seen/config", "");
    publish("binary_sensor/" + root + "/commissioning_pending/config", "");

    publish("sensor/" + root + "/target_current/config", "");
    publish("sensor/" + root + "/measured_current/config", "");
    publish("sensor/" + root + "/system_status/config", "");
    publish("sensor/" + root + "/system_config/config", "");

    publish("sensor/" + root + "/output_voltage/config", "");
    publish("sensor/" + root + "/output_current/config", "");
    publish("sensor/" + root + "/output_power/config", "");
    publish("sensor/" + root + "/temperature/config", "");
    publish("sensor/" + root + "/state_word/config", "");
    publish("sensor/" + root + "/alarm_word/config", "");

    publish("sensor/" + root + "/battery_voltage/config", "");
    publish("sensor/" + root + "/battery_current/config", "");
    publish("sensor/" + root + "/battery_power/config", "");
}

void MqttHandleHassClass::publishMeanwellNpb450Sensor(
    const String& name, const String& state_topic,
    const String& unit_of_measure, const String& icon,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    JsonDocument root;
    createMeanwellNpb450Info(root);
    publishSensor(root, getMeanwellNpb450RootDevice(), getMeanwellNpb450UniqueId(), name, state_topic, unit_of_measure, icon, device_class, state_class, category);
}

void MqttHandleHassClass::publishMeanwellNpb450BinarySensor(
    const String& name, const String& state_topic, const String& payload_on, const String& payload_off,
    const DeviceClassType device_class, const StateClassType state_class, const CategoryType category)
{
    JsonDocument root;
    createMeanwellNpb450Info(root);
    publishBinarySensor(root, getMeanwellNpb450RootDevice(), getMeanwellNpb450UniqueId(), name, state_topic, payload_on, payload_off, device_class, state_class, category);
}

void MqttHandleHassClass::publishMeanwellNpb450Switch(
    const String& name, const String& state_topic, const String& command_topic,
    const String& payload_on, const String& payload_off,
    const String& state_on, const String& state_off,
    const String& icon, const CategoryType category)
{
    String switchId = name;
    switchId.toLowerCase();
    switchId.replace(" ", "_");

    JsonDocument root;
    createMeanwellNpb450Info(root);

    root["name"] = name;
    root["uniq_id"] = getMeanwellNpb450UniqueId() + "_" + switchId;
    root["cmd_t"] = MqttSettings.getPrefix() + command_topic;
    root["stat_t"] = MqttSettings.getPrefix() + state_topic;
    root["payload_on"] = payload_on;
    root["payload_off"] = payload_off;
    root["state_on"] = state_on;
    root["state_off"] = state_off;

    addCommonMetadata(root, "", icon, DEVICE_CLS_NONE, STATE_CLS_NONE, category);

    const CONFIG_T& config = Configuration.get();
    root["avty_t"] = MqttSettings.getPrefix() + config.Mqtt.Lwt.Topic;
    root["pl_avail"] = config.Mqtt.Lwt.Value_Online;
    root["pl_not_avail"] = config.Mqtt.Lwt.Value_Offline;

    publish("switch/" + getMeanwellNpb450RootDevice() + "/" + switchId + "/config", root);
}

void MqttHandleHassClass::publishMeanwellNpb450Number(
    const String& name, const String& state_topic, const String& command_topic,
    const float min, const float max, const float step,
    const String& unit_of_measure, const String& icon,
    const StateClassType state_class, const CategoryType category)
{
    String numberId = name;
    numberId.toLowerCase();
    numberId.replace(" ", "_");

    JsonDocument root;
    createMeanwellNpb450Info(root);

    root["name"] = name;
    root["uniq_id"] = getMeanwellNpb450UniqueId() + "_" + numberId;
    root["cmd_t"] = MqttSettings.getPrefix() + command_topic;
    if (!state_topic.isEmpty()) {
        root["stat_t"] = MqttSettings.getPrefix() + state_topic;
    }
    root["min"] = min;
    root["max"] = max;
    root["step"] = step;

    addCommonMetadata(root, unit_of_measure, icon, DEVICE_CLS_NONE, state_class, category);

    const CONFIG_T& config = Configuration.get();
    root["avty_t"] = MqttSettings.getPrefix() + config.Mqtt.Lwt.Topic;
    root["pl_avail"] = config.Mqtt.Lwt.Value_Online;
    root["pl_not_avail"] = config.Mqtt.Lwt.Value_Offline;

    publish("number/" + getMeanwellNpb450RootDevice() + "/" + numberId + "/config", root);
}

void MqttHandleHassClass::publishMeanwellNpb450Button(
    const String& name, const String& command_topic, const String& payload,
    const String& icon, const CategoryType category)
{
    String buttonId = name;
    buttonId.toLowerCase();
    buttonId.replace(" ", "_");

    JsonDocument root;
    createMeanwellNpb450Info(root);

    root["name"] = name;
    root["uniq_id"] = getMeanwellNpb450UniqueId() + "_" + buttonId;
    root["cmd_t"] = MqttSettings.getPrefix() + command_topic;
    root["payload_press"] = payload;

    addCommonMetadata(root, "", icon, DEVICE_CLS_NONE, STATE_CLS_NONE, category);

    const CONFIG_T& config = Configuration.get();
    root["avty_t"] = MqttSettings.getPrefix() + config.Mqtt.Lwt.Topic;
    root["pl_avail"] = config.Mqtt.Lwt.Value_Online;
    root["pl_not_avail"] = config.Mqtt.Lwt.Value_Offline;

    publish("button/" + getMeanwellNpb450RootDevice() + "/" + buttonId + "/config", root);
}
