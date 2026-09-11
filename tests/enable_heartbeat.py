#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
# ]
# ///
"""Enable Heartbeat."""

from harp.device import core
from harp.serial import open_device
from harp.protocol import HarpMessage, MessageType, RegisterU16, RegisterU8
from typing import ClassVar
from threading import Event

SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number

received_heartbeats: int = 0
received_enough_heartbeats: Event = Event()
received_tstamps: int = 0
received_enough_tstamps: Event = Event()

class Heartbeat(RegisterU16):
    """A app register that does not exist on most devices."""
    address: ClassVar[int] = 18

def print_tstamp_second(msg: HarpMessage) -> None:
    global received_tstamps, received_enough_tstamps
    print(f"[{msg.address}] {msg.timestamp:.6f}  {msg.message_type.name:<5s}  {msg.payload}")
    received_tstamps += 1
    if received_tstamps == 3:
        received_enough_tstamps.set()


def print_heartbeat(msg: HarpMessage) -> None:
    global received_heartbeats, received_enough_heartbeats
    print(f"[{msg.address}] {msg.timestamp:.6f}  {msg.message_type.name:<5s}  {msg.payload}")
    received_heartbeats += 1
    if received_heartbeats == 3:
        received_enough_heartbeats.set()

with open_device(port=SERIAL_PORT) as device:
    device.subscribe(Heartbeat, print_heartbeat, message_types=MessageType.Event)
    device.subscribe(core.TimestampSeconds, print_tstamp_second, message_types=MessageType.Event)
    try:
        print("Enabling ALIVE_EN (which will dispatch the timestamp second register).")
        device.write(
                core.OperationControl,
                core.OperationControlPayload(
                    operation_mode=core.OperationMode.ACTIVE,
                    dump_registers=False,
                    heartbeat=core.EnableFlag.ENABLED, ## "ALIVE_EN" bit
                    mute_replies=False,
                    operation_led=core.EnableFlag.DISABLED,
                    visual_indicators=core.EnableFlag.ENABLED,
                )
            )
        received_enough_tstamps.wait(timeout=5)
        if not received_enough_tstamps.is_set():
            raise RuntimeError("Did not receive enough timestamp heartbeats.")
        print("Enabling HEARTBEAT_EN (which will dispatch the heartbeat register).")
        # Manually construct payload bc HEARTBEAT_EN bit is not exposed.
        raw_payload = int.from_bytes(core.OperationControlPayload(
                            operation_mode=core.OperationMode.ACTIVE,
                            dump_registers=False,
                            heartbeat=core.EnableFlag.ENABLED, ## "ALIVE_EN" bit
                            mute_replies=False,
                            operation_led=core.EnableFlag.DISABLED,
                            visual_indicators=core.EnableFlag.ENABLED,
                        ).payload_array.tobytes()) | 0x04 # patch in HEARTBEAT_EN
        device.write(RegisterU8(10), raw_payload)
        received_enough_heartbeats.wait(timeout=5)
        if not received_enough_heartbeats.is_set():
            raise RuntimeError("Did not receive enough heartbeat register messages.")
    except KeyboardInterrupt:
        pass
    print("Disconnecting.")
