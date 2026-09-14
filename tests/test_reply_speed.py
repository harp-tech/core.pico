#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "harp",
#     "numpy",
#     "matplotlib"
# ]
# ///
from harp.device import core
from harp.serial import open_device
import numpy as np
from time import sleep, perf_counter
from matplotlib import pyplot as plt


SERIAL_PORT = "/dev/ttyACM0"  # or "COMx" in Windows, where "x" is the serial port number
ROUND_TRIPS = 30000

timestamps_t = np.zeros(ROUND_TRIPS, dtype=float);

with open_device(port=SERIAL_PORT) as device:
    print(f"Performing {ROUND_TRIPS}x round trips. "
        "(Message from PC to Harp device. Reply from Harp device to PC.)")
    for i in range(ROUND_TRIPS):
        device_timestamp = device.read(core.OperationControl).timestamp
        timestamps_t[i] = perf_counter()

    time_deltas_t = np.diff(timestamps_t)
    print(f"Summary:")
    print(f"mean: {np.mean(time_deltas_t):.6f}")
    print(f"std dev: {np.std(time_deltas_t):.6f}")
    print(f"max: {np.max(time_deltas_t):.6f} at index: {np.argmax(time_deltas_t)}")
    print()
    large_value_locations = np.where(time_deltas_t > 0.003)
    print("The following delay times are large:")
    print(large_value_locations)
    print("indexes of the large delay times:")
    print([time_deltas_t[i] for i in large_value_locations])
    print("Disconnecting")


plt.hist(time_deltas_t, bins='auto')
plt.show()
