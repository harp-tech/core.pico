#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
# ]
# ///
"""Reboot to USB device."""

from harp.device import core
from harp.serial import open_device
from harp.device.client import TransportError


SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number

with open_device(port=SERIAL_PORT) as device:
    try:
        device.write(core.ResetDevice, core.ResetFlags.UPDATE_FIRMWARE)
    except TransportError:
        print("Device has disconnected and should reset as a mass-storage device.")
