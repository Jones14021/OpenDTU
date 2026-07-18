#!/usr/bin/env python3
"""Raw CAN zero-export simulator for OpenDTU + Mean Well NPB-450.

This script sends exact NPB command/data payloads to OpenDTU MQTT CAN bridge.
"""

from __future__ import annotations

import argparse
import json
import signal
import sys
import threading
import time
from dataclasses import dataclass
from typing import Any

import paho.mqtt.client as mqtt

NPB_CMD_OPERATION = 0x0000
NPB_CMD_VOUT_SET = 0x0020
NPB_CMD_IOUT_SET = 0x0030
NPB_CMD_SYSTEM_CONFIG = 0x00C2


@dataclass
class ControlState:
    grid_export_w: float = 0.0


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


class RawNpb450Controller:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.state = ControlState()
        self.running = True
        self.lock = threading.Lock()

        self.prefix = args.prefix
        self.tx_topic = self.prefix + "meanwell/can/tx"
        self.rx_topic = self.prefix + "meanwell/can/rx"
        self.grid_topic = args.grid_topic

        self.ctrl_id = 0x000C0100 + int(clamp(args.charger_address, 0, 15))

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=args.client_id)
        if args.username:
            self.client.username_pw_set(args.username, args.password)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any) -> None:
        print(f"[mqtt] connected: reason={reason_code}")
        client.subscribe(self.grid_topic, qos=0)
        client.subscribe(self.rx_topic, qos=0)

    def on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        payload = msg.payload.decode("utf-8", errors="replace").strip()

        if msg.topic == self.grid_topic:
            try:
                raw = float(payload)
            except ValueError:
                return

            export_w = raw if self.args.grid_export_positive else -raw
            with self.lock:
                self.state.grid_export_w = max(0.0, export_w)

    def send_command(self, command: int, data: int) -> None:
        frame = {
            "id": self.ctrl_id,
            "ext": True,
            "rtr": False,
            "dlc": 4,
            "data": [command & 0xFF, (command >> 8) & 0xFF, data & 0xFF, (data >> 8) & 0xFF],
        }
        self.client.publish(self.tx_topic, json.dumps(frame, separators=(",", ":")), qos=0, retain=False)

    def control_step(self) -> None:
        with self.lock:
            export_w = self.state.grid_export_w

        target_charge_w = clamp(export_w - self.args.deadband_w, 0.0, self.args.max_charge_power_w)
        target_current_a = clamp(
            target_charge_w / self.args.charge_voltage_v if self.args.charge_voltage_v > 0 else 0.0,
            0.0,
            self.args.max_charge_current_a,
        )

        vout_data = int(round(self.args.charge_voltage_v * 100.0))
        iout_data = int(round(target_current_a * 100.0))
        enabled = target_current_a >= self.args.min_enable_current_a

        self.send_command(NPB_CMD_VOUT_SET, vout_data)
        self.send_command(NPB_CMD_IOUT_SET, iout_data)
        self.send_command(NPB_CMD_OPERATION, 0x0001 if enabled else 0x0000)

        print(
            "[loop] export={:.1f}W target={:.1f}W current={:.2f}A voltage={:.2f}V enable={}".format(
                export_w,
                target_charge_w,
                target_current_a,
                self.args.charge_voltage_v,
                enabled,
            )
        )

    def run(self) -> None:
        self.client.connect(self.args.broker, self.args.port, keepalive=30)
        self.client.loop_start()

        # Runtime EEPROM lock for validation-focused runs.
        self.send_command(NPB_CMD_SYSTEM_CONFIG, 0x0400)

        try:
            while self.running:
                self.control_step()
                time.sleep(self.args.interval_s)
        finally:
            self.send_command(NPB_CMD_OPERATION, 0x0000)
            self.client.loop_stop()
            self.client.disconnect()


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Raw NPB-450 zero-export simulator over OpenDTU MQTT CAN bridge")
    p.add_argument("--broker", required=True)
    p.add_argument("--port", type=int, default=1883)
    p.add_argument("--username", default="")
    p.add_argument("--password", default="")
    p.add_argument("--client-id", default="npb450-raw-zero-export-sim")
    p.add_argument("--prefix", default="solar/")
    p.add_argument("--charger-address", type=int, default=0)
    p.add_argument("--grid-topic", required=True)
    p.add_argument("--grid-export-positive", action="store_true", default=True)
    p.add_argument("--grid-export-negative", action="store_false", dest="grid_export_positive")
    p.add_argument("--charge-voltage-v", type=float, default=14.4)
    p.add_argument("--max-charge-current-a", type=float, default=30.0)
    p.add_argument("--max-charge-power-w", type=float, default=430.0)
    p.add_argument("--deadband-w", type=float, default=40.0)
    p.add_argument("--min-enable-current-a", type=float, default=1.0)
    p.add_argument("--interval-s", type=float, default=2.0)
    return p


def main() -> int:
    args = build_arg_parser().parse_args()
    ctrl = RawNpb450Controller(args)

    def stop_handler(signum: int, frame: Any) -> None:
        ctrl.running = False

    signal.signal(signal.SIGINT, stop_handler)
    signal.signal(signal.SIGTERM, stop_handler)

    ctrl.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
