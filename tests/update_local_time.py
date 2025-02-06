#!/usr/bin/env python3
from enum import Enum
from pyharp.device import Device, DeviceMode
from pyharp.messages import HarpMessage
from pyharp.messages import MessageType
from pyharp.messages import CommonRegisters as CoreRegs
from struct import *
from time import sleep, perf_counter

COM_PORT = "/dev/ttyACM0" # COMxx on Windows.

device = Device(COM_PORT, "ibl.bin")


# Get the old time.
curr_time_s = device.send(HarpMessage.ReadU32(
                            CoreRegs.TIMESTAMP_SECOND).frame).payload[0]
print(f"Current seconds: {curr_time_s}")

# Update Harp time on the device.
set_time_seconds = int(3e9)
print(f"Setting Harp seconds to {set_time_seconds}")
_ = device.send(HarpMessage.WriteU32(CoreRegs.TIMESTAMP_SECOND,
                                     set_time_seconds).frame)
sleep(1)

# Get the new time from the device:
new_time_s = device.send(HarpMessage.ReadU32(
                            CoreRegs.TIMESTAMP_SECOND).frame).payload[0]
print(f"Updated seconds: {new_time_s}")

