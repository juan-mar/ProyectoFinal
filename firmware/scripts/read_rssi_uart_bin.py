#!/usr/bin/env python3
"""
Read binary RSSI dump from ESP32 over UART.

Protocol emitted by firmware command '&':
- 4 bytes magic: b'RSB1'
- uint32 little-endian: record_size (expected 8)
- uint32 little-endian: record_count
- payload: record_count binary records, each '<IhBB'

Record layout '<IhBB':
- uint32 millis
- int16 rssi
- uint8 mode   (0=calibration, 1=training)
- uint8 reserved
"""

import argparse
import csv
import struct
import sys
import time

import serial

MAGIC = b"RSB1"
HEADER_FMT = "<4sII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
RECORD_FMT = "<IhBB"
RECORD_SIZE = struct.calcsize(RECORD_FMT)


def read_exact(ser: serial.Serial, size: int, timeout_s: float) -> bytes:
    deadline = time.time() + timeout_s
    chunks = bytearray()
    while len(chunks) < size:
        if time.time() > deadline:
            raise TimeoutError(f"Timeout reading {size} bytes (got {len(chunks)})")
        data = ser.read(size - len(chunks))
        if data:
            chunks.extend(data)
    return bytes(chunks)


def find_magic_and_header(ser: serial.Serial, timeout_s: float):
    window = bytearray()
    deadline = time.time() + timeout_s

    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        window.extend(b)
        if len(window) > len(MAGIC):
            window.pop(0)
        if bytes(window) == MAGIC:
            rest = read_exact(ser, HEADER_SIZE - len(MAGIC), timeout_s)
            header = MAGIC + rest
            magic, record_size, record_count = struct.unpack(HEADER_FMT, header)
            return magic, record_size, record_count

    raise TimeoutError("Could not find dump magic RSB1 on UART")


def main():
    parser = argparse.ArgumentParser(description="Read binary RSSI dump from ESP32 UART and export CSV")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM6")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=8.0, help="Read timeout in seconds")
    parser.add_argument("--output", default="rssi_dump.csv", help="Output CSV path")
    parser.add_argument("--send", default="&", help="Command to trigger dump (default: '&')")
    args = parser.parse_args()

    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
            time.sleep(1.0)
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            ser.write(args.send.encode("ascii"))

            magic, record_size, record_count = find_magic_and_header(ser, args.timeout)
            if magic != MAGIC:
                raise RuntimeError("Invalid magic in dump header")
            if record_size != RECORD_SIZE:
                raise RuntimeError(
                    f"Unexpected record size {record_size}. Firmware expects {RECORD_SIZE}."
                )

            payload_size = record_size * record_count
            payload = read_exact(ser, payload_size, args.timeout)

        rows = []
        for i in range(record_count):
            start = i * record_size
            end = start + record_size
            millis, rssi, mode, _reserved = struct.unpack(RECORD_FMT, payload[start:end])
            mode_name = "CAL" if mode == 0 else "TRAIN" if mode == 1 else f"UNK({mode})"
            rows.append((millis, mode, mode_name, rssi))

        with open(args.output, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["millis", "mode", "mode_name", "rssi"])
            writer.writerows(rows)

        print(f"OK: {record_count} records saved to {args.output}")

    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
