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
2. [`examples/meanwell_npb450_can_profile.example.json`](examples/meanwell_npb450_can_profile.example.json): CAN ID/config profile.
3. [`examples/npb450_zero_export_sim.py`](examples/npb450_zero_export_sim.py): Python MQTT control loop example for zero-export style charging.
4. [`examples/README.md`](examples/README.md): end-to-end flow, CAN ID table, and Mermaid diagrams.

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
