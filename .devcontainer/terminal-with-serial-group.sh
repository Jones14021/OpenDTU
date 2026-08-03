#!/usr/bin/env bash
set -euo pipefail

# Prevent infinite recursion if the script is re-entered through sg/new shell.
if [[ "${OPENDTU_SERIAL_SHELL_ACTIVE:-0}" == "1" ]]; then
    exec /bin/bash -l
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

if device_path="$(find_serial_device 2>/dev/null)"; then
    device_gid="$(stat -c '%g' "${device_path}")"
    group_name="$(getent group "${device_gid}" | cut -d: -f1 || true)"

    if [[ -n "${group_name}" ]]; then
        if id -Gn | tr ' ' '\n' | grep -qx "${group_name}"; then
            exec /bin/bash -l
        fi

        if OPENDTU_SERIAL_SHELL_ACTIVE=1 sg "${group_name}" -c "exec /bin/bash -l"; then
            exit 0
        fi
    fi
fi

exec /bin/bash -l
