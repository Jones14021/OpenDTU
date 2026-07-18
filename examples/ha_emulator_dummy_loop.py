#!/usr/bin/env python3
"""Host-side Home Assistant emulator for OpenDTU NPB-450 validation.

Modes:
1) abstract: publish ON/OFF + target watts to OpenDTU abstract endpoints
2) raw: publish exact CAN frames to solar/meanwell/can/tx
"""

from __future__ import annotations

import argparse
import json
import math
import signal
import sys
import time
from typing import Any

import paho.mqtt.client as mqtt


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def build_raw_frame(command: int, data: int, can_id: int) -> dict[str, Any]:
    return {
        "id": can_id,
        "ext": True,
        "rtr": False,
        "dlc": 4,
        "data": [command & 0xFF, (command >> 8) & 0xFF, data & 0xFF, (data >> 8) & 0xFF],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Dummy HA emulator for OpenDTU NPB-450")
    parser.add_argument("--broker", required=True)
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--username", default="")
    parser.add_argument("--password", default="")
    parser.add_argument("--prefix", default="solar/")
    parser.add_argument("--mode", choices=["abstract", "raw"], default="abstract")
    parser.add_argument("--duration-s", type=int, default=300)
    parser.add_argument("--period-s", type=float, default=2.0)
    parser.add_argument("--max-target-w", type=float, default=430.0)
    parser.add_argument("--charge-voltage-v", type=float, default=14.4)
    parser.add_argument("--charger-address", type=int, default=0)
    args = parser.parse_args()

    running = True

    def stop_handler(signum: int, frame: Any) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop_handler)
    signal.signal(signal.SIGTERM, stop_handler)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="ha-emulator-npb450")
    if args.username:
        client.username_pw_set(args.username, args.password)
    client.connect(args.broker, args.port, keepalive=30)
    client.loop_start()

    can_tx_topic = f"{args.prefix}meanwell/can/tx"
    enable_topic = f"{args.prefix}meanwell/npb450/control/enable"
    target_w_topic = f"{args.prefix}meanwell/npb450/control/target_w"

    ctrl_id = 0x000C0100 + int(clamp(args.charger_address, 0, 15))

    # Boot sequence mirror for validation runs
    if args.mode == "raw":
        client.publish(can_tx_topic, json.dumps(build_raw_frame(0x00C2, 0x0400, ctrl_id)), qos=0, retain=False)

    client.publish(enable_topic, "ON", qos=0, retain=False)

    start = time.time()
    while running and (time.time() - start) < args.duration_s:
        t = time.time() - start
        simulated_export = 300.0 + 220.0 * math.sin(t / 30.0)
        target_w = clamp(simulated_export - 40.0, 0.0, args.max_target_w)

        if args.mode == "abstract":
            client.publish(target_w_topic, f"{target_w:.1f}", qos=0, retain=False)
        else:
            target_current = clamp(target_w / args.charge_voltage_v, 0.0, 40.0)
            vout_data = int(round(args.charge_voltage_v * 100.0))
            iout_data = int(round(target_current * 100.0))

            client.publish(can_tx_topic, json.dumps(build_raw_frame(0x0020, vout_data, ctrl_id)), qos=0, retain=False)
            client.publish(can_tx_topic, json.dumps(build_raw_frame(0x0030, iout_data, ctrl_id)), qos=0, retain=False)
            client.publish(can_tx_topic, json.dumps(build_raw_frame(0x0000, 0x0001, ctrl_id)), qos=0, retain=False)

        print(f"mode={args.mode} export_w={simulated_export:.1f} target_w={target_w:.1f}")
        time.sleep(args.period_s)

    if args.mode == "raw":
        client.publish(can_tx_topic, json.dumps(build_raw_frame(0x0000, 0x0000, ctrl_id)), qos=0, retain=False)

    client.publish(enable_topic, "OFF", qos=0, retain=False)
    client.publish(target_w_topic, "0", qos=0, retain=False)

    client.loop_stop()
    client.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
