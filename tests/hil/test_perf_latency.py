import re
import time

from conftest import send_cmd


def parse_frame_total(lines):
    for ln in lines:
        m = re.search(r"Frame Total:\s*(\d+)\s*", ln)
        if m:
            return int(m.group(1))
    return None


def test_perf_report_under_8ms(ser):
    # Ask the device for a perf dump
    lines = send_cmd(ser, "PERF", wait=0.5)

    frame_us = parse_frame_total(lines)
    assert frame_us is not None, f"No Frame Total found in output: {lines}"
    assert frame_us < 8000, f"Frame budget exceeded: {frame_us} us"


def test_stress_command_does_not_crash(ser):
    # Start stress test (non-blocking); ensure device responds sanely
    lines = send_cmd(ser, "PERF STRESS", wait=0.5)
    joined = "\n".join(lines)
    assert "STRESS" in joined or "Performance" in joined

