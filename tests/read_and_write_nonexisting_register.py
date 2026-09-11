#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
# ]
# ///
"""Read/Writing to a non-existent register should reply with a Read/Write Error."""

from harp.device import core
from harp.device.client import DeviceError
from harp.serial import open_device
from harp.protocol import HarpMessage, MessageType, RegisterU8
from typing import ClassVar


SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number


class NonexistentCoreRegister(RegisterU8):
    """A core register that does not exist."""
    address: ClassVar[int] = 31

class NonexistentAppRegister(RegisterU8):
    """A app register that does not exist on most devices."""
    address: ClassVar[int] = 255


with open_device(port=SERIAL_PORT) as device:
    # Test core regs
    try:
        print(f"Reading from nonexistent core register [{NonexistentCoreRegister.address}].")
        bad_read_reply = device.read(NonexistentCoreRegister)
        raise RuntimeError("Device did not raise an error when reading from non-existent register!")
    except DeviceError as e:
        print(f"OK! Device replied with an error: {e!s}")

    try:
        print(f"Writing to nonexistent core register [{NonexistentCoreRegister.address}].")
        bad_write_reply = device.write(NonexistentCoreRegister, 0x00)
        raise RuntimeError("Device did not raise an error when writing to non-existent register!")
    except DeviceError as e:
        print(f"OK! Device replied with an error: {e!s}")

    # Test app regs. (Note that this wont work if the device actually has 255 registers.)
        try:
            print(f"Reading from nonexistent app register [{NonexistentAppRegister.address}].")
            bad_read_reply = device.read(NonexistentAppRegister)
            raise RuntimeError("Device did not raise an error when reading from non-existent register!")
        except DeviceError as e:
            print(f"OK! Device replied with an error: {e!s}")

        try:
            print(f"Writing to nonexistent register [{NonexistentAppRegister.address}].")
            bad_write_reply = device.write(NonexistentAppRegister, 0x00)
            raise RuntimeError("Device did not raise an error when writing to non-existent register!")
        except DeviceError as e:
            print(f"OK! Device replied with an error: {e!s}")

    print("Disconnecting.")
