---
name: opendtu-hardware-workflow
description: "Use when building, validating, debugging, uploading, flashing, or using the OpenDTU hardware target. Covers the generic_esp32 PlatformIO build, WebSocket console logs, target health checks, OTA firmware upload, Web API verification, Hoymiles HMS-400-1T CMT radio behavior, live data, and cautious Meanwell NPB-450 MCP2515 CAN diagnostics."
argument-hint: "Describe the build, deployment, hardware test, or diagnostic task"
---

# OpenDTU Hardware Workflow

Use this skill for any action involving the configured target at `10.0.1.65`. Hardware access is valuable and potentially state-changing, so prove the local artifact first and minimize target interactions.

## Environment Constraint

- The devcontainer installs `curl` for authenticated Web API requests and `websocat` for direct console WebSocket access. `websocat` is installed from upstream release binaries because Debian bullseye does not provide an apt package. No relay or proxy process is required.
- Verify websocket tooling in a rebuilt container with `websocat --version`.
- After rebuilding the devcontainer, connect to the console with:

   ```sh
   websocat ws://admin:openDTU42@10.0.1.65/console
   ```

   Use this only after the target preflight succeeds. The target's console applies Digest authentication when read-only access is disabled; the supplied URL is the known development connection.

## Target Hardware

- Inverter: Hoymiles HMS-400-1T connected through the CMT2000 RF module. In the source tree and device profiles, this path is named `CMT2300A`.
- Charger: Mean Well NPB-450-12 connected through an MCP2515 CAN controller.
- The MCP2515 runs on a separate SPI bus and is initialized after the CMT radio. Treat radio and CAN pin mappings as coupled target configuration; do not replace the CMT mapping with an NRF24 mapping or reuse CAN pins.

## Build

1. Build from the repository root with:

   ```sh
   pio run -e generic_esp32
   ```

2. Do not substitute another PlatformIO environment unless the user explicitly asks. `generic_esp32` is the project default and produces `.pio/build/generic_esp32/firmware.bin`.
3. If the change affects `webapp/`, run `yarn build` from `webapp/` before the PlatformIO build. Confirm `webapp_dist/index.html.gz` and `webapp_dist/js/app.js.gz` changed as appropriate; PlatformIO embeds those files in the firmware.

## Target Preflight

1. Before every target-specific action, make a single authenticated health request:

   ```sh
   curl --digest -u admin:openDTU42 --fail --silent --show-error \
     http://10.0.1.65/api/system/status
   ```

   `GET /api/system/status` reports the active `pioenv`, firmware Git metadata, uptime, reset reasons, memory, filesystem, and radio state. Stop if it fails or identifies an unexpected target.
2. Use only the necessary follow-up endpoint for the task. Avoid polling and repeated target writes.
3. After a state-changing action, verify once with `/api/system/status` or `/api/livedata/status`, then inspect console logs if needed.

## Console and Live Diagnostics

- Firmware log WebSocket: `ws://admin:openDTU42@10.0.1.65/console`.
- The console is at `/console`. The `websocat` command in Environment Constraint is the direct terminal client; do not introduce a `socat` relay for normal log inspection.
- `GET /api/livedata/status` is the one-shot counterpart to the `/livedata` WebSocket and includes inverter data, totals, and configured charger/CAN status.
- `GET /api/devinfo/status` provides inverter firmware and hardware information.
- `GET /api/eventlog/status?inv=<serial>&locale=en` retrieves inverter events. Use a real inverter serial from live data.
- `GET /api/prometheus/metrics` exposes Prometheus-formatted metrics when that format is useful.

## OTA Firmware Update

The browser route `http://10.0.1.65/firmware/upgrade` uploads to `POST /api/firmware/update`.

1. Build successfully, then select `.pio/build/generic_esp32/firmware.bin`.
2. Compute the lowercase MD5 checksum and send a multipart request containing both required fields. Example:

   ```sh
   firmware=.pio/build/generic_esp32/firmware.bin
   md5=$(md5sum "$firmware" | awk '{print $1}')
   curl --digest -u admin:openDTU42 --fail --show-error \
     -F "MD5=$md5" -F "firmware=@$firmware;filename=firmware" \
     http://10.0.1.65/api/firmware/update
   ```

3. A successful upload returns `OK` with HTTP 200 and immediately schedules a restart. Do not make additional requests while it reboots.
4. Once available again, make one `/api/system/status` request and confirm `pioenv` is `generic_esp32`; inspect `/console` only if the status or expected behavior is wrong.

## Meanwell NPB/NPP-E Diagnostics

- Read `NPB,NPP-E.html` first; it is the official NPB/NPP-E reference kept in this repository.
- Also inspect `include/MeanwellCan.h`, `src/MeanwellCan.cpp`, the relevant `MeanwellCan` callers, and `examples/` device-profile files before changing behavior.
- Charger/CAN state is included in `/api/livedata/status`. Record a single before/after snapshot for targeted validation.
- `POST /api/livedata/charger/can_tx` can enqueue raw CAN traffic. It is a stateful diagnostic interface: use it only for a user-approved, minimal test after health preflight and with console monitoring.