#!/usr/bin/env python3
from enum import Enum
from pyharp.device import Device, DeviceMode
from pyharp.messages import HarpMessage
from pyharp.messages import MessageType
from pyharp.messages import CommonRegisters as CoreRegs
from struct import *
from time import perf_counter as now

COM_PORT = "/dev/ttyACM0" # COMxx on Windows.

device = Device(COM_PORT, "ibl.bin")


# Enable heartbeat
print("Enabling heartbeat.")
device.enable_heartbeat()

start_time_s = now()
duration_s = 3

while now() - start_time_s < duration_s:
    event_reply = device._read()
    if event_reply is not None:
        print()
        print(event_reply)

# Cleanup:
print("Disabling heartbeat.")
device.disable_heartbeat()
