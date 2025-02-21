#!/usr/bin/env python3
from pyharp.device import Device, DeviceMode
from pyharp.messages import HarpMessage
from pyharp.messages import MessageType
from pyharp.messages import CommonRegisters as Regs
from struct import unpack
import os

UUID_CORE_REGISTER = 16
TAG_CORE_REGISTER = 17


# ON THIS EXAMPLE
#
# This code opens the connection with the device and displays the information
# Also saves device's information into variables


# Open the device and print the info on screen
# Open serial connection and save communication to a file
if os.name == 'posix': # check for Linux.
    #device = Device("/dev/harp_device_00", "ibl.bin")
    device = Device("/dev/ttyACM0", "ibl.bin")
else: # assume Windows.
    device = Device("COM95", "ibl.bin")
device.info()                           # Display device's info on screen
try:
    # Get UUID and Git Tag (Not fully supported on all devices yet)
    reply = device.send(HarpMessage.ReadU8(UUID_CORE_REGISTER).frame)
    payload_64_bit_array = unpack('<QQ', bytearray(reply.payload))
    payload_hex = payload_64_bit_array[0] + (payload_64_bit_array[1] << 64)
    print(f"UUID: [{', '.join(hex(x) for x in reply.payload)}]")
    print(f"UUID: {payload_hex:032x}")
    reply = device.send(HarpMessage.ReadU8(TAG_CORE_REGISTER).frame)
    print(f"Git Tag: {reply.payload}")
except Exception as e:
    print(e)
# dump registers.
#print("Register dump:")
#print(device.dump_registers())
# Close connection
device.disconnect()
