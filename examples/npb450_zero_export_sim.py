#!/usr/bin/env python3
"""
Example MQTT-based zero-export controller for OpenDTU CAN bridge + Mean Well NPB-450.

Dependencies:
    pip install paho-mqtt

Usage:
    python3 examples/npb450_zero_export_sim.py \
      --broker 192.168.1.10 \
      --grid-topic home/grid/power_export_w \
      --profile examples/meanwell_npb450_can_profile.example.json
"""

from __future__ import annotations

import argparse
import json
import signal
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import paho.mqtt.client as mqtt


@dataclass
class ControlState:
    grid_export_w: float = 0.0
    last_status: dict[str, Any] | None = None


def parse_hex_or_int(value: str) -> int:
    value = value.strip()
    if value.lower().startswith("0x"):
        return int(value, 16)
    return int(value, 10)


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


class ZeroExportController:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.state = ControlState()
        self.running = True
        self.lock = threading.Lock()

        with open(args.profile, "r", encoding="utf-8") as f:
            self.profile = json.load(f)

        mqtt_cfg = self.profile["mqtt"]
        self.prefix = mqtt_cfg["prefix"]
        self.tx_topic = self.prefix + mqtt_cfg["tx_topic"]
        self.rx_topic = self.prefix + mqtt_cfg["rx_topic"]
        self.grid_topic = args.grid_topic

        enc = self.profile["encoding"]
        self.scale = int(enc["setpoint_scale"])
        self.setpoint_offset = int(enc["setpoint_offset"])
        self.setpoint_width = int(enc["setpoint_width"])
        self.dlc = int(enc["command_dlc"])
        self.endianness = enc["setpoint_endianness"].lower()
        self.enable_idx = int(enc["enable_byte_index"])
        self.enable_on = int(enc["enable_on_value"])
        self.enable_off = int(enc["enable_off_value"])

        tx_ids = self.profile["tx_ids"]
        self.id_set_current = parse_hex_or_int(tx_ids["set_charge_current"])
        self.id_set_voltage = parse_hex_or_int(tx_ids["set_charge_voltage"])
        self.id_enable = parse_hex_or_int(tx_ids["charger_enable"])

        rx_ids = self.profile["rx_ids"]
        self.rx_status_ids = {parse_hex_or_int(v) for v in rx_ids.values()}

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=args.client_id)
        if args.username:
            self.client.username_pw_set(args.username, args.password)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any) -> None:
        print(f"[mqtt] connected: reason={reason_code}")
        client.subscribe(self.grid_topic, qos=0)
        client.subscribe(self.rx_topic, qos=0)
        print(f"[mqtt] subscribed: {self.grid_topic}, {self.rx_topic}")

    def on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        payload = msg.payload.decode("utf-8", errors="replace").strip()

        if msg.topic == self.grid_topic:
            try:
                raw = float(payload)
            except ValueError:
                print(f"[warn] invalid grid topic payload: {payload}")
                return

            # Positive export is expected by default; invert if your meter uses opposite sign.
            export_w = raw if self.args.grid_export_positive else -raw
            with self.lock:
                self.state.grid_export_w = max(0.0, export_w)
            return

        if msg.topic == self.rx_topic:
            try:
                frame = json.loads(payload)
            except json.JSONDecodeError:
                print(f"[warn] invalid CAN RX JSON: {payload}")
                return

            frame_id = int(frame.get("id", -1))
            if frame_id not in self.rx_status_ids:
                return
            with self.lock:
                self.state.last_status = frame

    def pack_setpoint_frame(self, can_id: int, setpoint: float) -> dict[str, Any]:
        data = [0] * self.dlc
        value = int(round(max(0.0, setpoint) * self.scale))
        raw = value.to_bytes(self.setpoint_width, byteorder=self.endianness, signed=False)
        for i, b in enumerate(raw):
            data[self.setpoint_offset + i] = b
        return {"id": can_id, "ext": False, "rtr": False, "dlc": self.dlc, "data": data}

    def pack_enable_frame(self, enabled: bool) -> dict[str, Any]:
        data = [0] * self.dlc
        data[self.enable_idx] = self.enable_on if enabled else self.enable_off
        return {"id": self.id_enable, "ext": False, "rtr": False, "dlc": self.dlc, "data": data}

    def publish_can(self, frame: dict[str, Any]) -> None:
        self.client.publish(self.tx_topic, json.dumps(frame, separators=(",", ":")), qos=0, retain=False)

    def control_step(self) -> None:
        with self.lock:
            export_w = self.state.grid_export_w
            status = self.state.last_status

        target_charge_w = clamp(export_w - self.args.deadband_w, 0.0, self.args.max_charge_power_w)
        target_current_a = clamp(
            target_charge_w / self.args.charge_voltage_v if self.args.charge_voltage_v > 0 else 0.0,
            0.0,
            self.args.max_charge_current_a,
        )
        target_voltage_v = clamp(self.args.charge_voltage_v, 0.0, 60.0)

        charger_enable = target_current_a >= self.args.min_enable_current_a

        if self.id_set_current > 0:
            self.publish_can(self.pack_setpoint_frame(self.id_set_current, target_current_a))
        else:
            print("[warn] tx_ids.set_charge_current is 0x000; skipping current command")

        if self.id_set_voltage > 0:
            self.publish_can(self.pack_setpoint_frame(self.id_set_voltage, target_voltage_v))
        else:
            print("[warn] tx_ids.set_charge_voltage is 0x000; skipping voltage command")

        if self.id_enable > 0:
            self.publish_can(self.pack_enable_frame(charger_enable))
        else:
            print("[warn] tx_ids.charger_enable is 0x000; skipping enable command")

        print(
            "[loop] export={:.1f}W target={:.1f}W current={:.2f}A voltage={:.2f}V enable={} status_id={}".format(
                export_w,
                target_charge_w,
                target_current_a,
                target_voltage_v,
                charger_enable,
                status["id"] if status else "n/a",
            )
        )

    def run(self) -> None:
        self.client.connect(self.args.broker, self.args.port, keepalive=30)
        self.client.loop_start()
        print("[ctrl] zero-export loop started")
        try:
            while self.running:
                self.control_step()
                time.sleep(self.args.interval_s)
        finally:
            self.client.loop_stop()
            self.client.disconnect()
            print("[ctrl] stopped")


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Zero-export simulator for OpenDTU MQTT CAN bridge + NPB-450")
    p.add_argument("--broker", required=True, help="MQTT broker host/IP")
    p.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    p.add_argument("--username", default="", help="MQTT username")
    p.add_argument("--password", default="", help="MQTT password")
    p.add_argument("--client-id", default="npb450-zero-export-sim", help="MQTT client id")
    p.add_argument("--profile", type=Path, required=True, help="Path to CAN profile JSON")
    p.add_argument("--grid-topic", required=True, help="MQTT topic with current export power in W")
    p.add_argument("--grid-export-positive", action="store_true", default=True, help="Grid topic value > 0 means export")
    p.add_argument(
        "--grid-export-negative",
        action="store_false",
        dest="grid_export_positive",
        help="Grid topic value < 0 means export",
    )
    p.add_argument("--charge-voltage-v", type=float, default=14.2, help="LiFePO4 charge voltage setpoint")
    p.add_argument("--max-charge-current-a", type=float, default=30.0, help="NPB-450 current limit")
    p.add_argument("--max-charge-power-w", type=float, default=430.0, help="Power cap for control loop")
    p.add_argument("--deadband-w", type=float, default=40.0, help="Ignore export below this threshold")
    p.add_argument("--min-enable-current-a", type=float, default=1.0, help="Disable charger below this current")
    p.add_argument("--interval-s", type=float, default=2.0, help="Control loop period")
    return p


def main() -> int:
    args = build_arg_parser().parse_args()
    if not args.profile.exists():
        print(f"profile not found: {args.profile}")
        return 2

    ctrl = ZeroExportController(args)

    def stop_handler(signum: int, frame: Any) -> None:
        ctrl.running = False

    signal.signal(signal.SIGINT, stop_handler)
    signal.signal(signal.SIGTERM, stop_handler)
    ctrl.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
