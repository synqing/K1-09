import os
import sys
import time
import glob
from contextlib import contextmanager

import serial
import pytest

BAUD = int(os.environ.get("SB_BAUD", "230400"))
DEFAULT_GLOBS = [
    "/dev/cu.usbmodem*",
    "/dev/cu.usbserial*",
    "/dev/ttyACM*",
    "/dev/ttyUSB*",
]

def _auto_detect_port():
    override = os.environ.get("SB_SERIAL_PORT")
    if override:
        return override
    for pat in DEFAULT_GLOBS:
        matches = sorted(glob.glob(pat))
        if matches:
            return matches[0]
    raise RuntimeError("No serial port found. Set SB_SERIAL_PORT=/dev/xxx")


@pytest.fixture(scope="session")
def serial_port():
    port = _auto_detect_port()
    return port


@pytest.fixture(scope="function")
def ser(serial_port):
    with serial.Serial(serial_port, BAUD, timeout=1.0) as s:
        # give device a moment
        time.sleep(0.2)
        # flush any banner
        s.reset_input_buffer()
        yield s

def send_cmd(ser, cmd: str, wait=0.2):
    ser.write((cmd + "\n").encode("ascii"))
    ser.flush()
    time.sleep(wait)
    lines = []
    while True:
        line = ser.readline()
        if not line:
            break
        try:
            lines.append(line.decode("utf-8", "ignore").rstrip())
        except Exception:
            pass
    return lines

