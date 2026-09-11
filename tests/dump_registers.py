#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
# ]
# ///
"""Dump all registers."""

from harp.device import core
from harp.serial import open_device
from harp.protocol import HarpMessage, MessageType


SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number


def print_any_reply(msg: HarpMessage) -> None:
    register = core.REGISTER_MAP.get(msg.address, None)
    value = register.parse(msg) if register is not None else msg.payload_bytes.hex()
    print(f"[{msg.address}] {msg.timestamp:.6f}  {msg.message_type.name:<5s}  {value}")


with open_device(port=SERIAL_PORT) as device:
    device.subscribe_all(print_any_reply, message_types=MessageType.Read)
    try:
        print("Dumping all registers.")
        print("Listening to read replies. Press Enter to stop.")
        device.write(
                core.OperationControl,
                core.OperationControlPayload(
                    operation_mode=core.OperationMode.ACTIVE,
                    dump_registers=True,
                    heartbeat=core.EnableFlag.DISABLED,
                    mute_replies=False,
                    operation_led=core.EnableFlag.ENABLED,
                    visual_indicators=core.EnableFlag.ENABLED,
                ),
            )
        input()
    except KeyboardInterrupt:
        pass
    print("Disconnecting.")
