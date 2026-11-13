"""Observe a flashed initialized HW-950 starter; optionally calibrate and require touch.

Upload with `vynec run --port COM5` from the generated project first.
Then run `python scripts/test_hw950_init.py --port COM5 --seconds 120 --calibrate --require-touch`.
Calibration is written only when explicitly requested; existing NVS is never erased.
"""
import argparse
import json
from pathlib import Path
import re
import os
import subprocess
import sys
import time


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--calibrate", action="store_true")
    parser.add_argument("--require-touch", action="store_true")
    parser.add_argument("--background", action="store_true", help="Detach capture so the operator can interact with the screen")
    parser.add_argument("--output", type=Path, help="Existing directory for serial.log and results.json")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.output and not args.output.is_dir():
        parser.error("--output must be an existing directory")
    if args.background:
        if not args.output:
            parser.error("--background requires --output")
        child_args = [sys.executable, str(Path(__file__).resolve()),
                      *(arg for arg in sys.argv[1:] if arg != "--background")]
        options = {"creationflags": subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
        with (args.output / "observation.console.log").open("w", encoding="utf-8") as stdout, \
             (args.output / "observation.error.log").open("w", encoding="utf-8") as stderr:
            child = subprocess.Popen(child_args, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                     close_fds=True, **options)
        print(f"HW-950 capture started: PID {child.pid}; results in {args.output}", flush=True)
        return 0
    chunks = []
    started = time.monotonic()
    sent_calibration = False
    with serial.Serial(args.port, 115200, timeout=0.1) as uart:
        uart.dtr = False
        uart.rts = True
        time.sleep(0.1)
        uart.reset_input_buffer()
        uart.rts = False
        while time.monotonic() - started < args.seconds:
            text = uart.readline().decode(errors="replace")
            if not text:
                continue
            chunks.append(text)
            print(text.rstrip(), flush=True)
            if args.calibrate and not sent_calibration and "HW950_TOUCH configured=true" in text:
                print("CALIBRATION REQUESTED: hold/release each of the four magenta targets", flush=True)
                uart.write(b"C")
                uart.flush()
                sent_calibration = True
    text = "".join(chunks)
    geometry = re.search(r"HW950_GEOMETRY rotation=(\d+) width=(\d+) height=(\d+)", text)
    contacts = [tuple(map(int, match)) for match in re.findall(r"HW950_CONTACT x=(\d+) y=(\d+) pressure=(\d+)", text)]
    heartbeats = [tuple(map(int, match)) for match in re.findall(r"HW950_HEARTBEAT hits=(\d+) arena=(\d+)", text)]
    expected = tuple(map(int, geometry.groups())) if geometry else None
    valid_contacts = expected is not None and all(0 <= x < expected[1] and 0 <= y < expected[2] and pressure > 0
                                                  for x, y, pressure in contacts)
    calibration_ok = "HW950_CALIBRATION_RESULT true" in text
    result = {
        "port": args.port, "geometry": expected, "configured": "HW950_TOUCH configured=true" in text,
        "calibrated_at_start": "calibrated=true" in text, "calibration_requested": args.calibrate,
        "calibration_completed": calibration_ok, "contacts": len(contacts),
        "contacts_in_bounds": valid_contacts, "heartbeats": len(heartbeats),
        "arena_observations": sorted(set(arena for _, arena in heartbeats)),
        "visual_alignment": "requires owner confirmation",
    }
    ok = result["configured"] and expected is not None and len(heartbeats) >= 2
    if args.calibrate:
        ok = ok and calibration_ok
    if args.require_touch:
        ok = ok and bool(contacts) and valid_contacts
    result["result"] = "PASS" if ok else "FAIL"
    print(json.dumps(result, indent=2), flush=True)
    if args.output:
        (args.output / "serial.log").write_text(text, encoding="utf-8")
        (args.output / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
