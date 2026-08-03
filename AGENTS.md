# OpenDTU Agent Guidance

## Scope and Build

- Default to the `generic_esp32` PlatformIO environment for every firmware build, verification, upload, and debug task. Do not select another board environment unless the user explicitly requests it.
- Build firmware with `pio run -e generic_esp32` from the repository root. The resulting firmware is in `.pio/build/generic_esp32/`.
- Keep changes narrowly scoped. Run the most relevant local validation before considering a hardware action.

## Web App Assets

- For any change below `webapp/`, run `yarn build` from `webapp/` before a firmware build.
- Confirm the generated embedded assets in `webapp_dist/`, especially `index.html.gz` and `js/app.js.gz`, are updated. `platformio.ini` embeds those gzip files into firmware; source-only UI changes are not deployable.

## Hardware Target: 10.0.1.65

- Treat runs against the target as scarce: first verify it is reachable with `curl --basic -u admin:openDTU42 --fail --silent --show-error http://10.0.1.65/api/system/status`. Do not upload, restart, change configuration, or transmit test commands if that check fails. Read-only access may be enabled, but explicit Basic authentication verifies the credentials required by state-changing API routes.
- Use the target only when local build and task-specific checks already pass. Prefer one deliberate verification run over repeated probing.
- Inspect firmware logs through `ws://admin:openDTU42@10.0.1.65/console`. The firmware's console endpoint is `/console`; when read-only access is disabled it uses Digest authentication.
- OTA firmware updates are performed through the web UI at `http://10.0.1.65/firmware/upgrade` or its backing `POST /api/firmware/update`. The HTTP API route uses Basic authentication; see the `opendtu-hardware-workflow` skill for the required multipart request and post-update checks.

## WebSocket Tooling

- The devcontainer installs `websocat` from upstream release binaries (not apt, because Debian bullseye has no `websocat` package).
- Verify tooling after container rebuild with `websocat --version`.
- Connect to the OpenDTU console WebSocket with `websocat ws://admin:openDTU42@10.0.1.65/console` after the `/api/system/status` preflight succeeds.
- `websocat -n1` captures only one WebSocket frame and can miss an asynchronous diagnostic. For a finite log window, use `timeout 20 websocat ws://admin:openDTU42@10.0.1.65/console`; do not use unsupported forms such as `-n50`.

## Installed Hardware

- The target communicates with a Hoymiles HMS-400-1T through the CMT2000 RF module. The firmware source and device profiles refer to this radio path as `CMT2300A`; preserve the CMT configuration when changing inverter or pin-mapping behavior.
- The target controls a Mean Well NPB-450-12 through an MCP2515 CAN controller. The CAN service uses its own SPI bus and initializes after the CMT radio; preserve that ordering and avoid reusing its configured CAN pins.

## Meanwell NPB/NPP-E

- Before changing Meanwell NPB-related behavior, consult `NPB,NPP-E.html`, the bundled official reference document, along with `include/MeanwellCan.h`, `src/MeanwellCan.cpp`, and the applicable device-profile examples.
- Avoid raw charger CAN transmissions except for an explicitly requested, minimal hardware test. Capture the before/after charger data from `/api/livedata/status` and console logs.