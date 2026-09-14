#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
# ]
# ///
"""Test name-change functionality."""

from harp.device import core
from harp.serial import open_device
from harp.protocol import HarpMessage


SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number


def print_any_reply(msg: HarpMessage) -> None:
    register = core.REGISTER_MAP.get(msg.address, None)
    value = register.parse(msg) if register is not None else msg.payload_bytes.hex()
    print(f"[{msg.address}] {msg.timestamp:.6f}  {msg.message_type.name:<5s}  {value}")


with open_device(port=SERIAL_PORT) as device:
    new_name = "jimothy"
    reply = device.read(core.DeviceName)
    print(f"Device name is: {reply.payload}")
    print(f"Changing name to: {new_name}")
    reply = device.write(core.DeviceName, new_name)
    reply = device.read(core.DeviceName)
    print(f"Device new name is: {reply.payload}")
    print("Resetting name.")
    device.write(core.ResetDevice, core.ResetFlags.RESTORE_NAME)
    print("Disconnecting.")
