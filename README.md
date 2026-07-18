# OpenDTU

[![OpenDTU Build](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml)
[![cpplint](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml)
[![Yarn Prettier](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnprettier.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnprettier.yml)

## !! IMPORTANT UPGRADE NOTES !!

If you are upgrading from a version before 15.03.2023 you have to upgrade the partition table of the ESP32. Please follow the [this](docs/UpgradePartition.md) documentation!

## Background

This project was started from [this](https://www.mikrocontroller.net/topic/525778) discussion (Mikrocontroller.net).
It was the goal to replace the original Hoymiles DTU (Telemetry Gateway) with their cloud access. With a lot of reverse engineering the Hoymiles protocol was decrypted and analyzed.

## Documentation

The documentation can be found [here](https://tbnobody.github.io/OpenDTU-docs/).
Please feel free to support and create a PR in [this](https://github.com/tbnobody/OpenDTU-docs) repository to make the documentation even better.

## Custom Use Case: Hoymiles + Mean Well NPB-450 + Zero Export

This repository can be used as a combined gateway for:

1. Hoymiles inverter communication over CMT2300A.
2. Mean Well NPB-450 charger communication over MCP2515 CAN.
3. Zero net export control by publishing CAN commands through MQTT.

For this setup, use the examples in [`examples/`](examples):

1. [`examples/device_profile_npb450_gateway.json`](examples/device_profile_npb450_gateway.json): board pin mapping (CMT + ETH + MCP2515).
2. [`examples/meanwell_npb450_can_profile.example.json`](examples/meanwell_npb450_can_profile.example.json): optional profile template for custom raw workflows.
3. [`examples/npb450_zero_export_sim.py`](examples/npb450_zero_export_sim.py): Python MQTT control loop example for zero-export style charging.
4. [`examples/README.md`](examples/README.md): end-to-end flow, CAN ID table, and Mermaid diagrams.
5. [`examples/home_assistant_npb450_package.yaml`](examples/home_assistant_npb450_package.yaml): Home Assistant package for abstract NPB-450 control.
6. [`examples/ha_emulator_dummy_loop.py`](examples/ha_emulator_dummy_loop.py): host-side Home Assistant emulator for validation.

### NPB-450 Integration Modes

This repository now supports two parallel integration modes for the charger:

1. Raw CAN mode: external controller publishes exact frame payloads.
2. Abstract mode: external controller publishes only `enable` and `target_w`; OpenDTU handles NPB-450 protocol details internally.

#### 1) Raw CAN Mode (Exact Message Ownership in HA/Host)

Use this mode if you want Home Assistant (or another external service) to fully own the command sequence and payload format.

Primary topic:

1. `solar/meanwell/can/tx` (publish frame JSON)
2. `solar/meanwell/can/rx` (receive frame JSON)

Frame JSON contract:

```json
{
  "id": 786688,
  "ext": true,
  "rtr": false,
  "dlc": 4,
  "data": [194, 0, 0, 4]
}
```

This path is suitable for strict protocol conformance tests, commissioning routines, and low-level debugging.

#### 2) Abstract Mode (OpenDTU Owns NPB-450 Protocol)

Use this mode if you want stable high-level control from Home Assistant without composing CAN payloads.

Control topics:

1. `solar/meanwell/npb450/control/enable`
2. `solar/meanwell/npb450/control/target_w`
3. `solar/meanwell/npb450/control/commission_psu` (one-shot commissioning command)

Configuration topics:

1. `solar/meanwell/npb450/config/charge_voltage_v`
2. `solar/meanwell/npb450/config/max_current_a`
3. `solar/meanwell/npb450/config/address`

Status topics:

1. `solar/meanwell/npb450/status/init_state`
2. `solar/meanwell/npb450/status/eeprom_lock_ok`
3. `solar/meanwell/npb450/status/psu_mode_ok`
4. `solar/meanwell/npb450/status/target_w`
5. `solar/meanwell/npb450/status/target_iout`
6. `solar/meanwell/npb450/status/target_vout`
7. `solar/meanwell/npb450/status/iout_actual`
8. `solar/meanwell/npb450/status/control_enabled`
9. `solar/meanwell/npb450/status/address`

### NPB-450-Specific Firmware Behavior in OpenDTU

In abstract mode, `MeanwellCan` now performs:

1. Boot initialization sequence with EEPROM lock command (`SYSTEM_CONFIG` write).
2. Validation polling for PSU mode and EEPROM lock state.
3. Internal state machine (`set_eeprom_lock` -> `request_validation` -> `ready` / `fault`).
4. Runtime conversion from target watts to current setpoint (`I = P / V`).
5. Periodic NPB command generation (`VOUT_SET`, `IOUT_SET`, `OPERATION`).

This design keeps high-frequency control writes in RAM-path operation while still allowing explicit commissioning when requested.

### Home Assistant Integration

For a ready-to-adapt HA package, use:

1. [`examples/home_assistant_npb450_package.yaml`](examples/home_assistant_npb450_package.yaml)

For host-side loop emulation (without full HA stack), use:

1. [`examples/ha_emulator_dummy_loop.py`](examples/ha_emulator_dummy_loop.py)

This emulator supports both raw and abstract modes and is useful to validate OpenDTU behavior before deploying automations.

### Safety Notes (12V 330Ah LiFePO4)

Suggested starting points for the provided examples:

1. Charge voltage around `14.2-14.4V`.
2. Max current at or below `30A` for NPB-450 envelope.
3. Target power limit around `430W`.
4. Enforce BMS limits and stop criteria in your automation policy.

Always verify charger addressing, DIP switch profile, and commissioning procedure before unattended operation.

## Breaking changes

Generated using: `git log --date=short --pretty=format:"* %h%x09%ad%x09%s" | grep BREAKING`

```code
* 8cab3335      2025-08-07      BREAKING CHANGE: WebAPI endpoint `/api/limit/config` requires different parameters
* 8372deaf      2025-04-18      BREAKING CHANGE: Logging newline changed from "\r\n" to "\n"
* 1b637f08      2024-01-30      BREAKING CHANGE: Web API Endpoint /api/livedata/status and /api/prometheus/metrics
* e1564780      2024-01-30      BREAKING CHANGE: Web API Endpoint /api/livedata/status and /api/prometheus/metrics
* f0b5542c      2024-01-30      BREAKING CHANGE: Web API Endpoint /api/livedata/status and /api/prometheus/metrics
* c27ecc36      2024-01-29      BREAKING CHANGE: Web API Endpoint /api/livedata/status
* 71d1b3b       2023-11-07      BREAKING CHANGE: Home Assistant Auto Discovery to new naming scheme
* 04f62e0       2023-04-20      BREAKING CHANGE: Web API Endpoint /api/eventlog/status no nested serial object
* 59f43a8       2023-04-17      BREAKING CHANGE: Web API Endpoint /api/devinfo/status requires GET parameter inv=
* 318136d       2023-03-15      BREAKING CHANGE: Updated partition table: Make sure you have a configuration backup and completly reflash the device!
* 3b7aef6       2023-02-13      BREAKING CHANGE: Web API!
* d4c838a       2023-02-06      BREAKING CHANGE: Prometheus API!
* daf847e       2022-11-14      BREAKING CHANGE: Removed deprecated config parsing method
* 69b675b       2022-11-01      BREAKING CHANGE: Structure WebAPI /api/livedata/status changed
* 27ed4e3       2022-10-31      BREAKING: Change power factor from percent value to value between 0 and 1
```

## Currently supported Inverters

A list of all currently supported inverters can be found [here](https://www.opendtu.solar/hardware/inverter_overview/)
