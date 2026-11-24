#!/usr/bin/env python3
"""Capture the Meshtastic serial log for a fixed time, surviving board resets.

`pio device monitor` needs an interactive terminal, so it cannot be used from scripts.
Usage (PlatformIO's Python ships pyserial):
    ~/.platformio/penv/bin/python -I docs/custom-sensor/tools/serlog.py /dev/ttyACM0 120 /path/to/out.log
Filter afterwards, e.g.:
    grep -a "TrapSensor" out.log | sed 's/\x1b\[[0-9;]*m//g'
"""
import sys
import time

import serial

port, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
end = time.time() + secs
with open(out, "wb") as f:
    while time.time() < end:
        try:
            s = serial.Serial(port, 115200, timeout=0.5)
        except Exception:
            time.sleep(0.2)  # port gone (reset / USB re-enumeration), retry
            continue
        try:
            while time.time() < end:
                d = s.read(4096)
                if d:
                    f.write(d)
                    f.flush()
        except Exception:
            f.write(b"\n--- port lost, reconnecting ---\n")
            f.flush()
        finally:
            try:
                s.close()
            except Exception:
                pass
