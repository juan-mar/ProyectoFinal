#!/usr/bin/env python3
"""
Live RSSI capture from UART (COM port) with CSV logging and real-time plot.

Expected input over serial (one sample per line):
1) millis,mode,rssi   -> e.g. 123456,0,-67
2) millis,rssi        -> e.g. 123456,-67
3) rssi               -> e.g. -67

CSV output columns:
pc_time_iso,millis,mode,rssi

Usage example:
python scripts/rssi_uart_live.py --port COM6 --baud 115200 --csv rssi_live.csv --window 100
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import re
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import serial

LINE_3_RE = re.compile(r"^\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*$")
LINE_2_RE = re.compile(r"^\s*(\d+)\s*,\s*(-?\d+)\s*$")
LINE_1_RE = re.compile(r"^\s*(-?\d+)\s*$")


@dataclass
class Sample:
    millis: int
    mode: int
    rssi: int
    pc_ts: str


class SharedState:
    def __init__(self, window: int) -> None:
        self.lock = threading.Lock()
        self.samples: deque[Sample] = deque(maxlen=window)
        self.total_samples = 0
        self.total_bad_lines = 0


class CsvWriter:
    def __init__(self, csv_path: Path) -> None:
        self._file = csv_path.open("a", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)

        if csv_path.stat().st_size == 0:
            self._writer.writerow(["pc_time_iso", "millis", "mode", "rssi"])
            self._file.flush()

    def write(self, sample: Sample) -> None:
        self._writer.writerow([sample.pc_ts, sample.millis, sample.mode, sample.rssi])
        self._file.flush()

    def close(self) -> None:
        self._file.close()


def parse_line(line: str, fallback_millis: int) -> Optional[Sample]:
    now_iso = dt.datetime.now().isoformat(timespec="milliseconds")

    m3 = LINE_3_RE.match(line)
    if m3:
        millis = int(m3.group(1))
        mode = int(m3.group(2))
        rssi = int(m3.group(3))
        return Sample(millis=millis, mode=mode, rssi=rssi, pc_ts=now_iso)

    m2 = LINE_2_RE.match(line)
    if m2:
        millis = int(m2.group(1))
        rssi = int(m2.group(2))
        return Sample(millis=millis, mode=255, rssi=rssi, pc_ts=now_iso)

    m1 = LINE_1_RE.match(line)
    if m1:
        rssi = int(m1.group(1))
        return Sample(millis=fallback_millis, mode=255, rssi=rssi, pc_ts=now_iso)

    return None


def reader_thread(
    ser: serial.Serial,
    state: SharedState,
    csv_writer: CsvWriter,
    stop_evt: threading.Event,
    verbose_bad_lines: bool,
) -> None:
    while not stop_evt.is_set():
        try:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            sample = parse_line(line, fallback_millis=int(time.time() * 1000))
            if sample is None:
                with state.lock:
                    state.total_bad_lines += 1
                if verbose_bad_lines:
                    print(f"[WARN] Unparsed line: {line}")
                continue

            csv_writer.write(sample)

            with state.lock:
                state.samples.append(sample)
                state.total_samples += 1

        except serial.SerialException as exc:
            print(f"[ERROR] Serial error: {exc}", file=sys.stderr)
            stop_evt.set()
            return
        except Exception as exc:
            print(f"[ERROR] Reader exception: {exc}", file=sys.stderr)
            stop_evt.set()
            return


def main() -> int:
    parser = argparse.ArgumentParser(description="Live RSSI UART capture to CSV + real-time plot")
    parser.add_argument("--port", required=True, help="COM port, e.g. COM6")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate (default: 115200)")
    parser.add_argument("--csv", default="rssi_live.csv", help="CSV output path")
    parser.add_argument("--window", type=int, default=100, help="Points to show in live plot")
    parser.add_argument("--refresh-ms", type=int, default=100, help="Plot refresh period in ms")
    parser.add_argument("--start-cmd", default="", help="Optional command sent at start (example: '&')")
    parser.add_argument(
        "--verbose-bad-lines",
        action="store_true",
        help="Print lines that do not match expected format",
    )
    args = parser.parse_args()

    csv_path = Path(args.csv)

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as exc:
        print(f"[ERROR] Cannot open serial port {args.port}: {exc}", file=sys.stderr)
        return 1

    print(f"[INFO] Connected to {args.port} @ {args.baud}")

    time.sleep(1.0)
    ser.reset_input_buffer()

    if args.start_cmd:
        ser.write(args.start_cmd.encode("ascii", errors="ignore"))
        print(f"[INFO] Start command sent: {args.start_cmd}")

    state = SharedState(window=args.window)
    writer = CsvWriter(csv_path)
    stop_evt = threading.Event()

    thread = threading.Thread(
        target=reader_thread,
        args=(ser, state, writer, stop_evt, args.verbose_bad_lines),
        daemon=True,
    )
    thread.start()

    fig, ax = plt.subplots(figsize=(10, 5))
    line, = ax.plot([], [], lw=1.5)
    ax.set_title("RSSI Live (last N points)")
    ax.set_xlabel("Sample index (window)")
    ax.set_ylabel("RSSI (dBm)")
    ax.grid(True, alpha=0.35)

    status_text = ax.text(0.01, 0.99, "", transform=ax.transAxes, va="top", ha="left")

    def update(_frame_idx: int):
        with state.lock:
            data = list(state.samples)
            total = state.total_samples
            bad = state.total_bad_lines

        if not data:
            return line, status_text

        y = [s.rssi for s in data]
        x = list(range(len(y)))

        line.set_data(x, y)

        y_min = min(y) - 2
        y_max = max(y) + 2
        if y_min == y_max:
            y_min -= 1
            y_max += 1

        ax.set_xlim(0, max(1, len(y) - 1))
        ax.set_ylim(y_min, y_max)

        last = data[-1]
        mode_text = "CAL" if last.mode == 0 else "TRAIN" if last.mode == 1 else "UNK"
        status_text.set_text(
            f"total={total}  bad={bad}  last_rssi={last.rssi} dBm  mode={mode_text}  millis={last.millis}"
        )

        return line, status_text

    anim = FuncAnimation(fig, update, interval=args.refresh_ms, blit=False, cache_frame_data=False)

    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        stop_evt.set()
        thread.join(timeout=1.0)
        writer.close()
        ser.close()
        _ = anim

    print(f"[INFO] Closed. CSV saved: {csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
