#!/usr/bin/env bash

set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FACTORY_BIN="${SCRIPT_DIR}/.pio/build/generic_esp32/firmware.factory.bin"

die() {
	echo "Error: $*" >&2
	exit 1
}

log() {
	echo "$*"
}

check_esptool() {
	if command -v esptool.py >/dev/null 2>&1; then
		ESPTOOL_CMD=(esptool.py)
		return
	fi

	if command -v esptool >/dev/null 2>&1; then
		ESPTOOL_CMD=(esptool)
		return
	fi

	if python3 -m esptool --help >/dev/null 2>&1; then
		ESPTOOL_CMD=(python3 -m esptool)
		return
	fi

	die "esptool is not installed. Install esptool.py or make sure 'python3 -m esptool' works."
}

add_candidate() {
	local device_path="$1"
	local display_name="$2"

	if [[ -n "${candidate_seen[$device_path]:-}" ]]; then
		return
	fi

	candidate_seen["$device_path"]=1
	candidate_paths+=("$device_path")
	candidate_labels+=("$display_name")
}

collect_candidates() {
	local serial_link
	local device_path
	local resolved_path

	for serial_link in /dev/serial/by-id/*; do
		[[ -e "$serial_link" ]] || continue
		resolved_path="$(readlink -f -- "$serial_link")"
		[[ -e "$resolved_path" ]] || continue
		add_candidate "$resolved_path" "$serial_link -> $resolved_path"
	done

	for device_path in /dev/ttyUSB* /dev/ttyACM*; do
		[[ -e "$device_path" ]] || continue
		add_candidate "$device_path" "$device_path"
	done
}

choose_device() {
	collect_candidates

	if (( ${#candidate_paths[@]} == 0 )); then
		log "No likely flashable serial device was found."
		read -r -p "Enter the serial device manually: " selected_device
		[[ -n "$selected_device" ]] || die "No device selected."
		[[ -e "$selected_device" ]] || die "Device does not exist: $selected_device"
		flash_device="$selected_device"
		return
	fi

	if (( ${#candidate_paths[@]} == 1 )); then
		flash_device="${candidate_paths[0]}"
		log "Detected one likely flashable device: ${candidate_labels[0]}"
	else
		log "Multiple likely flashable devices were found:"
		local i
		for i in "${!candidate_paths[@]}"; do
			printf '  %d) %s\n' "$((i + 1))" "${candidate_labels[$i]}"
		done

		local choice
		while true; do
			read -r -p "Choose a device by number: " choice
			if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#candidate_paths[@]} )); then
				flash_device="${candidate_paths[$((choice - 1))]}"
				break
			fi
			echo "Invalid choice."
		done
	fi
}

confirm_selection() {
	log "Selected device: $flash_device"
	read -r -p "Flash ${FACTORY_BIN##*/} to this device? [y/N] " answer
	case "$answer" in
		y|Y|yes|YES)
			;;
		*)
			die "Aborted by user."
			;;
	esac
}

main() {
	[[ -f "$FACTORY_BIN" ]] || die "Factory bin not found: $FACTORY_BIN"

	check_esptool

	declare -gA candidate_seen=()
	declare -ga candidate_paths=()
	declare -ga candidate_labels=()
	declare -g flash_device=""
	declare -g selected_device=""

	choose_device
	confirm_selection

	log "Starting flash with: ${ESPTOOL_CMD[*]}"
	log "Writing ${FACTORY_BIN} to ${flash_device}"

	if "${ESPTOOL_CMD[@]}" --chip esp32 --port "$flash_device" write_flash 0x0 "$FACTORY_BIN"; then
		log "Status: success"
	else
		local status=$?
		log "Status: failed (exit ${status})"
		exit "$status"
	fi
}

main "$@"