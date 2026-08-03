---
name: opendtu-webapp-assets
description: "Use when changing the OpenDTU Vue webapp, UI, frontend, routes, styles, translations, or firmware-facing browser behavior. Ensures type checking and Vite packaging regenerate the gzip assets embedded in generic_esp32 firmware."
argument-hint: "Describe the webapp change to implement or verify"
---

# OpenDTU Web App Asset Workflow

The Vue application in `webapp/` is not served from its source tree on devices. PlatformIO embeds the generated files from `webapp_dist/` into the firmware.

## Build and Verify

1. Make the UI change in `webapp/`.
2. From `webapp/`, run:

   ```sh
   yarn build
   ```

   This runs the TypeScript check and Vite production build.
3. Confirm `webapp_dist/index.html.gz` and `webapp_dist/js/app.js.gz` exist and are regenerated. The Vite compression configuration intentionally removes the uncompressed build outputs.
4. Build the firmware from the repository root with:

   ```sh
   pio run -e generic_esp32
   ```

5. For on-device UI verification, follow the preflight and OTA procedure in the `opendtu-hardware-workflow` skill. Verify target availability through `/api/system/status` before the upload.

## API and WebSocket Changes

- Keep browser API requests aligned with their handlers in `src/WebApi_*.cpp`.
- The development server proxies `/api` to HTTP and `/livedata` and `/console` to WebSockets. Do not treat a local Vite preview as proof that the embedded gzip assets were refreshed.
- Firmware upgrades require multipart `MD5` and `firmware` fields at `POST /api/firmware/update`; preserve that contract when modifying the upgrade screen.
- Prefer a one-shot `GET /api/livedata/status` for initial or post-action verification. Use the `/livedata` WebSocket for continuous display only.