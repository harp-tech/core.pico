#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
# ]
# ///
"""Change local time (assumes a synchronizer is not connected)."""

from harp.device import core
from harp.serial import open_device
from harp.protocol import HarpMessage, MessageType


SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number

with open_device(port=SERIAL_PORT) as device:
    old_time_s = device.read(core.TimestampSeconds).payload
    print(f"Old time is: {old_time_s}")
    device.write(core.TimestampSeconds, 1000)
    new_time_s = device.read(core.TimestampSeconds).payload
    print(f"New time is: {new_time_s}")
    print("Disconnecting.")
