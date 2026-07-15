#!/usr/bin/env bash
set -euo pipefail

TARGET_USER="${1:-vscode}"

if ! id "${TARGET_USER}" >/dev/null 2>&1; then
    echo "[serial-group] User '${TARGET_USER}' not found; skipping."
    exit 0
fi

find_serial_device() {
    local candidate=""
    local resolved=""

    for candidate in /dev/serial/by-id/* /dev/ttyUSB* /dev/ttyACM*; do
        [[ -e "${candidate}" ]] || continue

        if [[ -L "${candidate}" ]]; then
            resolved="$(readlink -f "${candidate}" 2>/dev/null || true)"
        else
            resolved="${candidate}"
        fi

        [[ -n "${resolved}" && -c "${resolved}" ]] || continue
        echo "${resolved}"
        return 0
    done

    return 1
}

if ! device_path="$(find_serial_device)"; then
    echo "[serial-group] No serial device found; skipping dynamic group mapping."
    exit 0
fi

device_gid="$(stat -c '%g' "${device_path}")"
if id -G "${TARGET_USER}" | tr ' ' '\n' | grep -qx "${device_gid}"; then
    echo "[serial-group] ${TARGET_USER} already has GID ${device_gid} for ${device_path}."
    exit 0
fi

group_name="$(getent group "${device_gid}" | cut -d: -f1 || true)"
if [[ -z "${group_name}" ]]; then
    group_name="hostserial_${device_gid}"
    groupadd -g "${device_gid}" "${group_name}"
    echo "[serial-group] Created group ${group_name} (GID ${device_gid})."
fi

usermod -aG "${group_name}" "${TARGET_USER}"
echo "[serial-group] Added ${TARGET_USER} to group ${group_name} (GID ${device_gid}) for ${device_path}."
echo "[serial-group] Open a new terminal session to refresh group membership."