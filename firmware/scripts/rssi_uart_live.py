#!/usr/bin/env python3
"""
Live RSSI capture from UART with protocol lines DAT/EVT/CFG.

For each run, the script creates:
scripts/rssi_csv/<run_timestamp>/
    - <run_timestamp>_calib.csv
    - <run_timestamp>_train.csv
    - <run_timestamp>_cfg.csv
    - <run_timestamp>_evt.csv

Expected UART protocol (positional fields):
- DAT,<millis>,<tipo>,<rssi_t1>,<cmp_t1>
- EVT,<millis>,<state_in_out>
- CFG,<millis>,<q>,<r>,<x0>,<p0>,<media_calib>,<varianza_calib>,<histeresis_in>,<histeresis_out>[,<estado_inicial>]

Also supports legacy numeric lines for transition period.
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
import math
from pathlib import Path
from typing import Dict, Optional

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import serial

from kalman_scalar import ScalarKalmanFilter

LINE_3_RE = re.compile(r"^\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*$")
LINE_2_RE = re.compile(r"^\s*(\d+)\s*,\s*(-?\d+)\s*$")
LINE_1_RE = re.compile(r"^\s*(-?\d+)\s*$")


@dataclass
class DataSample:
    millis: int
    tipo: str
    rssi_t1: float
    cmp_t1: Optional[float]
    py_t1: float
    py_dist_m: Optional[float]
    pc_ts: str


@dataclass
class EventSample:
    millis: int
    state: str
    raw_evt: str
    pc_ts: str


@dataclass
class ConfigSample:
    millis: int
    kalman_q: str
    kalman_r: str
    kalman_x0: str
    kalman_p0: str
    media_calib: str
    varianza_calib: str
    histeresis_in: str
    histeresis_out: str
    estado_inicial: str
    raw_cfg: str
    pc_ts: str


@dataclass
class FilterResult:
    value: float


class BaseFilter:
    def process(self, measurement: float) -> FilterResult:
        raise NotImplementedError()


class NoFilter(BaseFilter):
    def process(self, measurement: float) -> FilterResult:
        z = float(measurement)
        return FilterResult(value=z)


class ScalarKalmanAdapter(BaseFilter):
    def __init__(self, q: float, r: float, x0: float, p0: float) -> None:
        self.kf = ScalarKalmanFilter(q=q, r=r, x0=x0, p0=p0)

    def process(self, measurement: float) -> FilterResult:
        step = self.kf.step(measurement)
        return FilterResult(value=step.x_post)


def build_filter(name: str, q: float, r: float, x0: float, p0: float) -> BaseFilter:
    if name == "none":
        return NoFilter()
    if name == "kalman":
        return ScalarKalmanAdapter(q=q, r=r, x0=x0, p0=p0)
    if name == "ukf":
        print("[WARN] UKF not implemented yet. Falling back to filter=none.")
        return NoFilter()
    raise ValueError(f"Unsupported filter: {name}")


def normalize_tipo(value: str) -> str:
    v = value.strip().upper()
    if v in ("0", "CAL", "CALIB", "CALIBRATION"):
        return "CAL"
    if v in ("1", "TRAIN", "TRAINING"):
        return "TRAIN"
    return "UNK"


def normalize_state(value: str) -> str:
    v = value.strip().upper()
    if v in ("IN", "INSIDE", "ADENTRO", "1"):
        return "IN"
    if v in ("OUT", "OUTSIDE", "AFUERA", "0"):
        return "OUT"
    return "UNK"


def split_fields(line: str) -> list[str]:
    return [p.strip() for p in line.split(",")]


class SharedState:
    def __init__(self, window: int) -> None:
        self.lock = threading.Lock()
        self.samples: deque[DataSample] = deque(maxlen=window)
        self.total_samples = 0
        self.total_bad_lines = 0
        self.total_unknown_mode_lines = 0
        self.total_cal_samples = 0
        self.total_train_samples = 0
        self.total_cfg_lines = 0
        self.total_evt_lines = 0
        self.last_state = "UNK"
        self.hysteresis_in = 0.0
        self.hysteresis_out = 0.0
        self.calib_rssi_ref: Optional[float] = None


class DataCsvWriter:
    def __init__(self, csv_path: Path, overwrite: bool = False) -> None:
        mode = "w" if overwrite else "a"
        self._file = csv_path.open(mode, newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)

        if overwrite or csv_path.stat().st_size == 0:
            self._writer.writerow(
                [
                    "pc_time_iso",
                    "millis",
                    "tipo",
                    "rssi_t1",
                    "cmp_t1",
                    "py_t1",
                    "py_dist_m",
                ]
            )
            self._file.flush()

    def write(self, sample: DataSample) -> None:
        self._writer.writerow(
            [
                sample.pc_ts,
                sample.millis,
                sample.tipo,
                f"{sample.rssi_t1:.6f}",
                "" if sample.cmp_t1 is None else f"{sample.cmp_t1:.6f}",
                f"{sample.py_t1:.6f}",
                "" if sample.py_dist_m is None else f"{sample.py_dist_m:.6f}",
            ]
        )
        self._file.flush()

    def close(self) -> None:
        self._file.close()


@dataclass
class ParsedDatLine:
    millis: int
    tipo: str
    rssi_t1: float
    cmp_t1: Optional[float]


@dataclass
class ParsedEvtLine:
    millis: int
    state: str
    raw_evt: str


@dataclass
class ParsedCfgLine:
    millis: int
    kalman_q: str
    kalman_r: str
    kalman_x0: str
    kalman_p0: str
    media_calib: str
    varianza_calib: str
    histeresis_in: str
    histeresis_out: str
    estado_inicial: str
    raw_cfg: str


class ConfigCsvWriter:
    def __init__(self, csv_path: Path) -> None:
        self._file = csv_path.open("a", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        if csv_path.stat().st_size == 0:
            self._writer.writerow(
                [
                    "pc_time_iso",
                    "millis",
                    "kalman_q",
                    "kalman_r",
                    "kalman_x0",
                    "kalman_p0",
                    "media_calib",
                    "varianza_calib",
                    "histeresis_in",
                    "histeresis_out",
                    "estado_inicial",
                    "raw_cfg",
                ]
            )
            self._file.flush()

    def write(self, sample: ConfigSample) -> None:
        self._writer.writerow(
            [
                sample.pc_ts,
                sample.millis,
                sample.kalman_q,
                sample.kalman_r,
                sample.kalman_x0,
                sample.kalman_p0,
                sample.media_calib,
                sample.varianza_calib,
                sample.histeresis_in,
                sample.histeresis_out,
                sample.estado_inicial,
                sample.raw_cfg,
            ]
        )
        self._file.flush()

    def close(self) -> None:
        self._file.close()


class EventCsvWriter:
    def __init__(self, csv_path: Path) -> None:
        self._file = csv_path.open("a", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        if csv_path.stat().st_size == 0:
            self._writer.writerow(["pc_time_iso", "millis", "state", "raw_evt"])
            self._file.flush()

    def write(self, sample: EventSample) -> None:
        self._writer.writerow([sample.pc_ts, sample.millis, sample.state, sample.raw_evt])
        self._file.flush()

    def close(self) -> None:
        self._file.close()


def parse_dat_line(line: str, fallback_millis: int) -> Optional[ParsedDatLine]:
    fields = split_fields(line)
    if len(fields) >= 4 and fields[0].upper() == "DAT":
        try:
            millis = int(fields[1])
            tipo = normalize_tipo(fields[2])
            rssi_t1 = float(fields[3])
            cmp_t1 = float(fields[4]) if len(fields) >= 5 and fields[4] != "" else None
            return ParsedDatLine(millis=millis, tipo=tipo, rssi_t1=rssi_t1, cmp_t1=cmp_t1)
        except ValueError:
            return None

    # Legacy numeric fallback
    m3 = LINE_3_RE.match(line)
    if m3:
        millis = int(m3.group(1))
        mode = m3.group(2)
        rssi = float(m3.group(3))
        return ParsedDatLine(millis=millis, tipo=normalize_tipo(mode), rssi_t1=rssi, cmp_t1=None)

    m2 = LINE_2_RE.match(line)
    if m2:
        millis = int(m2.group(1))
        rssi = float(m2.group(2))
        return ParsedDatLine(millis=millis, tipo="UNK", rssi_t1=rssi, cmp_t1=None)

    m1 = LINE_1_RE.match(line)
    if m1:
        rssi = float(m1.group(1))
        return ParsedDatLine(millis=fallback_millis, tipo="UNK", rssi_t1=rssi, cmp_t1=None)

    return None


def parse_evt_line(line: str, fallback_millis: int) -> Optional[ParsedEvtLine]:
    fields = split_fields(line)
    if len(fields) < 3 or fields[0].upper() != "EVT":
        return None
    try:
        millis = int(fields[1]) if fields[1] else fallback_millis
    except ValueError:
        millis = fallback_millis
    state = normalize_state(fields[2])
    return ParsedEvtLine(millis=millis, state=state, raw_evt=line)


def parse_cfg_line(line: str, fallback_millis: int) -> Optional[ParsedCfgLine]:
    fields = split_fields(line)
    if len(fields) < 10 or fields[0].upper() != "CFG":
        return None
    try:
        millis = int(fields[1]) if fields[1] else fallback_millis
    except ValueError:
        millis = fallback_millis
    estado_inicial = normalize_state(fields[10]) if len(fields) >= 11 else "UNK"
    return ParsedCfgLine(
        millis=millis,
        kalman_q=fields[2],
        kalman_r=fields[3],
        kalman_x0=fields[4],
        kalman_p0=fields[5],
        media_calib=fields[6],
        varianza_calib=fields[7],
        histeresis_in=fields[8],
        histeresis_out=fields[9],
        estado_inicial=estado_inicial,
        raw_cfg=line,
    )


def try_parse_cfg_kalman(cfg: ParsedCfgLine) -> Optional[tuple[float, float, float, float]]:
    try:
        q = float(cfg.kalman_q)
        r = float(cfg.kalman_r)
        x0 = float(cfg.kalman_x0)
        p0 = float(cfg.kalman_p0)
    except ValueError:
        return None
    return q, r, x0, p0


def estimate_distance_m(
    rssi_value: float,
    ref_rssi_at_calib: Optional[float],
    calibration_distance_m: float,
    path_loss_exp: float,
) -> Optional[float]:
    if ref_rssi_at_calib is None or calibration_distance_m <= 0.0 or path_loss_exp <= 0.0:
        return None
    exponent = (ref_rssi_at_calib - rssi_value) / (10.0 * path_loss_exp)
    return calibration_distance_m * (10.0 ** exponent)


def reader_thread(
    ser: serial.Serial,
    state: SharedState,
    mode_writers: Dict[str, DataCsvWriter],
    mode_filters: Dict[str, BaseFilter],
    unknown_writer: DataCsvWriter,
    unknown_filter: BaseFilter,
    cfg_writer: ConfigCsvWriter,
    evt_writer: EventCsvWriter,
    stop_evt: threading.Event,
    verbose_bad_lines: bool,
    kalman_source: str,
    calibration_distance_m: float,
    path_loss_exp: float,
) -> None:
    while not stop_evt.is_set():
        try:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            now_ms = int(time.time() * 1000)

            cfg = parse_cfg_line(line, fallback_millis=now_ms)
            if cfg is not None:
                cfg_writer.write(
                    ConfigSample(
                        millis=cfg.millis,
                        kalman_q=cfg.kalman_q,
                        kalman_r=cfg.kalman_r,
                        kalman_x0=cfg.kalman_x0,
                        kalman_p0=cfg.kalman_p0,
                        media_calib=cfg.media_calib,
                        varianza_calib=cfg.varianza_calib,
                        histeresis_in=cfg.histeresis_in,
                        histeresis_out=cfg.histeresis_out,
                        estado_inicial=cfg.estado_inicial,
                        raw_cfg=cfg.raw_cfg,
                        pc_ts=dt.datetime.now().isoformat(timespec="milliseconds"),
                    )
                )
                with state.lock:
                    state.total_cfg_lines += 1
                    try:
                        state.hysteresis_in = float(cfg.histeresis_in)
                    except ValueError:
                        pass
                    try:
                        state.hysteresis_out = float(cfg.histeresis_out)
                    except ValueError:
                        pass
                    try:
                        state.calib_rssi_ref = float(cfg.media_calib)
                    except ValueError:
                        pass

                if kalman_source == "fw-cfg":
                    parsed_kalman = try_parse_cfg_kalman(cfg)
                    if parsed_kalman is None:
                        if verbose_bad_lines:
                            print(f"[WARN] CFG ignored (invalid q/r/x0/p0): {cfg.raw_cfg}")
                    else:
                        q, r, x0, p0 = parsed_kalman
                        mode_filters["CAL"] = build_filter("kalman", q, r, x0, p0)
                        mode_filters["TRAIN"] = build_filter("kalman", q, r, x0, p0)
                        unknown_filter = build_filter("kalman", q, r, x0, p0)
                        print(
                            "[INFO] Kalman params updated from CFG "
                            f"(Q={q}, R={r}, x0={x0}, p0={p0})"
                        )
                continue

            evt = parse_evt_line(line, fallback_millis=now_ms)
            if evt is not None:
                evt_writer.write(
                    EventSample(
                        millis=evt.millis,
                        state=evt.state,
                        raw_evt=evt.raw_evt,
                        pc_ts=dt.datetime.now().isoformat(timespec="milliseconds"),
                    )
                )
                with state.lock:
                    state.total_evt_lines += 1
                    state.last_state = evt.state
                continue

            parsed = parse_dat_line(line, fallback_millis=now_ms)
            if parsed is None:
                with state.lock:
                    state.total_bad_lines += 1
                if verbose_bad_lines:
                    print(f"[WARN] Unparsed line: {line}")
                continue

            if parsed.tipo not in ("CAL", "TRAIN"):
                py_t1 = unknown_filter.process(parsed.rssi_t1).value
                with state.lock:
                    calib_rssi_ref = state.calib_rssi_ref
                py_dist_m = estimate_distance_m(
                    rssi_value=py_t1,
                    ref_rssi_at_calib=calib_rssi_ref,
                    calibration_distance_m=calibration_distance_m,
                    path_loss_exp=path_loss_exp,
                )
                sample = DataSample(
                    millis=parsed.millis,
                    tipo="UNK",
                    rssi_t1=parsed.rssi_t1,
                    cmp_t1=parsed.cmp_t1,
                    py_t1=py_t1,
                    py_dist_m=py_dist_m,
                    pc_ts=dt.datetime.now().isoformat(timespec="milliseconds"),
                )
                unknown_writer.write(sample)
                with state.lock:
                    state.samples.append(sample)
                    state.total_samples += 1
                    state.total_unknown_mode_lines += 1
                if verbose_bad_lines:
                    print(f"[WARN] Unknown tipo saved to unknown CSV: {line}")
                continue

            py_t1 = mode_filters[parsed.tipo].process(parsed.rssi_t1).value
            with state.lock:
                calib_rssi_ref = state.calib_rssi_ref
            py_dist_m = estimate_distance_m(
                rssi_value=py_t1,
                ref_rssi_at_calib=calib_rssi_ref,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )

            sample = DataSample(
                millis=parsed.millis,
                tipo=parsed.tipo,
                rssi_t1=parsed.rssi_t1,
                cmp_t1=parsed.cmp_t1,
                py_t1=py_t1,
                py_dist_m=py_dist_m,
                pc_ts=dt.datetime.now().isoformat(timespec="milliseconds"),
            )

            mode_writers[parsed.tipo].write(sample)

            with state.lock:
                state.samples.append(sample)
                state.total_samples += 1
                if parsed.tipo == "CAL":
                    state.total_cal_samples += 1
                else:
                    state.total_train_samples += 1

        except serial.SerialException as exc:
            print(f"[ERROR] Serial error: {exc}", file=sys.stderr)
            stop_evt.set()
            return
        except Exception as exc:
            print(f"[ERROR] Reader exception: {exc}", file=sys.stderr)
            stop_evt.set()
            return


def main() -> int:
    parser = argparse.ArgumentParser(description="Live UART capture for DAT/EVT/CFG with per-run CSV files")
    parser.add_argument("--port", required=True, help="COM port, e.g. COM6")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate (default: 115200)")
    parser.add_argument(
        "--output-root",
        default="",
        help="Root output folder for run directories (default: <script_dir>/rssi_csv)",
    )
    parser.add_argument("--window", type=int, default=100, help="Points to show in live plot")
    parser.add_argument("--refresh-ms", type=int, default=100, help="Plot refresh period in ms")
    parser.add_argument("--start-cmd", default="", help="Optional command sent at start (example: '&')")
    parser.add_argument("--filter", choices=["none", "kalman", "ukf"], default="none", help="Fallback filter when DAT line has no cmp_t1")
    parser.add_argument(
        "--kalman-source",
        choices=["cli", "fw-cfg"],
        default="cli",
        help="When --filter kalman: use CLI params or update params from incoming CFG lines",
    )
    parser.add_argument("--kalman-q", type=float, default=2.0, help="Kalman process noise Q")
    parser.add_argument("--kalman-r", type=float, default=9.0, help="Kalman measurement noise R")
    parser.add_argument("--kalman-x0", type=float, default=-60.0, help="Kalman initial state x0")
    parser.add_argument("--kalman-p0", type=float, default=100.0, help="Kalman initial covariance p0")
    parser.add_argument(
        "--calibration-distance-m",
        type=float,
        default=1.0,
        help="Distance in meters used during calibration to interpret CFG media_calib",
    )
    parser.add_argument(
        "--path-loss-exp",
        type=float,
        default=2.0,
        help="Path-loss exponent n for RSSI->distance conversion",
    )
    parser.add_argument(
        "--verbose-bad-lines",
        action="store_true",
        help="Print lines that do not match expected format",
    )
    args = parser.parse_args()

    run_stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    script_dir = Path(__file__).resolve().parent
    output_root = Path(args.output_root) if args.output_root else (script_dir / "rssi_csv")
    run_dir = output_root / run_stamp
    run_dir.mkdir(parents=True, exist_ok=True)
    calib_csv = run_dir / f"{run_stamp}_calib.csv"
    train_csv = run_dir / f"{run_stamp}_train.csv"
    cfg_csv = run_dir / f"{run_stamp}_cfg.csv"
    evt_csv = run_dir / f"{run_stamp}_evt.csv"
    unknown_csv = output_root / "unknown_mode_latest.csv"

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as exc:
        print(f"[ERROR] Cannot open serial port {args.port}: {exc}", file=sys.stderr)
        return 1

    print(f"[INFO] Connected to {args.port} @ {args.baud}")
    print(
        "[INFO] Distance model "
        f"(d_cal={args.calibration_distance_m} m, n={args.path_loss_exp})"
    )

    time.sleep(1.0)
    ser.reset_input_buffer()

    if args.start_cmd:
        ser.write(args.start_cmd.encode("ascii", errors="ignore"))
        print(f"[INFO] Start command sent: {args.start_cmd}")

    mode_filters: Dict[str, BaseFilter] = {
        "CAL": build_filter(args.filter, args.kalman_q, args.kalman_r, args.kalman_x0, args.kalman_p0),
        "TRAIN": build_filter(args.filter, args.kalman_q, args.kalman_r, args.kalman_x0, args.kalman_p0),
    }
    unknown_filter = build_filter(args.filter, args.kalman_q, args.kalman_r, args.kalman_x0, args.kalman_p0)
    print(f"[INFO] Filter selected: {args.filter}")
    if args.filter == "kalman":
        print(
            "[INFO] Kalman params "
            f"(Q={args.kalman_q}, R={args.kalman_r}, x0={args.kalman_x0}, p0={args.kalman_p0})"
        )
        print(f"[INFO] Kalman source: {args.kalman_source}")
        if args.kalman_source == "fw-cfg":
            print("[INFO] Waiting for CFG lines to override CLI Kalman params")

    mode_writers = {
        "CAL": DataCsvWriter(calib_csv),
        "TRAIN": DataCsvWriter(train_csv),
    }
    unknown_writer = DataCsvWriter(unknown_csv, overwrite=True)
    cfg_writer = ConfigCsvWriter(cfg_csv)
    evt_writer = EventCsvWriter(evt_csv)
    print(f"[INFO] Run directory: {run_dir}")
    print(f"[INFO] CAL CSV: {calib_csv}")
    print(f"[INFO] TRAIN CSV: {train_csv}")
    print(f"[INFO] CFG CSV: {cfg_csv}")
    print(f"[INFO] EVT CSV: {evt_csv}")
    print(f"[INFO] UNKNOWN CSV (overwritten each run): {unknown_csv}")

    state = SharedState(window=args.window)
    stop_evt = threading.Event()

    thread = threading.Thread(
        target=reader_thread,
        args=(
            ser,
            state,
            mode_writers,
            mode_filters,
            unknown_writer,
            unknown_filter,
            cfg_writer,
            evt_writer,
            stop_evt,
            args.verbose_bad_lines,
            args.kalman_source,
            args.calibration_distance_m,
            args.path_loss_exp,
        ),
        daemon=True,
    )
    thread.start()

    fig, ax = plt.subplots(figsize=(10, 5))
    raw_line, = ax.plot([], [], lw=1.2, label="RSSI crudo", color="#1f77b4")
    cmp_line, = ax.plot([], [], lw=2.0, label="CMP t1 (firmware)", color="#d62728")
    py_line, = ax.plot([], [], lw=1.8, label="Filtro Python", color="#2ca02c")
    h_in_line = ax.axhline(y=0.0, color="#9467bd", linestyle="--", linewidth=1.2, label="Histeresis IN")
    h_out_line = ax.axhline(y=0.0, color="#8c564b", linestyle=":", linewidth=1.2, label="Histeresis OUT")
    ax.set_title("UART Live (raw vs cmp firmware vs python)")
    ax.set_xlabel("Sample index (window)")
    ax.set_ylabel("RSSI (dBm)")
    ax.grid(True, alpha=0.35)
    ax.legend(loc="lower right")

    status_text = ax.text(0.01, 0.99, "", transform=ax.transAxes, va="top", ha="left")

    def update(_frame_idx: int):
        with state.lock:
            data = list(state.samples)
            total = state.total_samples
            bad = state.total_bad_lines
            unknown = state.total_unknown_mode_lines
            n_cal = state.total_cal_samples
            n_train = state.total_train_samples
            n_cfg = state.total_cfg_lines
            n_evt = state.total_evt_lines
            last_state = state.last_state
            h_in = state.hysteresis_in
            h_out = state.hysteresis_out
            calib_rssi_ref = state.calib_rssi_ref

        if not data:
            return raw_line, cmp_line, py_line, h_in_line, h_out_line, status_text

        y_raw = [s.rssi_t1 for s in data]
        y_cmp = [float("nan") if s.cmp_t1 is None else s.cmp_t1 for s in data]
        y_py = [s.py_t1 for s in data]
        x = list(range(len(y_raw)))

        raw_line.set_data(x, y_raw)
        cmp_line.set_data(x, y_cmp)
        py_line.set_data(x, y_py)
        h_in_line.set_ydata([h_in, h_in])
        h_out_line.set_ydata([h_out, h_out])

        y_cmp_valid = [v for v in y_cmp if not math.isnan(v)]
        y_all_min = [min(y_raw), min(y_py), h_in, h_out]
        y_all_max = [max(y_raw), max(y_py), h_in, h_out]
        if y_cmp_valid:
            y_all_min.append(min(y_cmp_valid))
            y_all_max.append(max(y_cmp_valid))

        y_min = min(y_all_min) - 2
        y_max = max(y_all_max) + 2
        if y_min == y_max:
            y_min -= 1
            y_max += 1

        ax.set_xlim(0, max(1, len(y_raw) - 1))
        ax.set_ylim(y_min, y_max)

        last = data[-1]
        status_text.set_text(
            "total="
            f"{total} cal={n_cal} train={n_train} cfg={n_cfg} evt={n_evt} bad={bad} unknown={unknown}  "
            f"raw={last.rssi_t1:.1f} dBm  "
            f"cmp_fw={(last.cmp_t1 if last.cmp_t1 is not None else float('nan')):.2f} dBm  "
            f"cmp_py={last.py_t1:.2f} dBm  "
            f"dist_py={(last.py_dist_m if last.py_dist_m is not None else float('nan')):.2f} m  "
            f"h_in={h_in:.2f} h_out={h_out:.2f} tipo={last.tipo} state={last_state} millis={last.millis}"
            f" calib_ref={(calib_rssi_ref if calib_rssi_ref is not None else float('nan')):.2f}"
        )

        return raw_line, cmp_line, py_line, h_in_line, h_out_line, status_text

    anim = FuncAnimation(fig, update, interval=args.refresh_ms, blit=False, cache_frame_data=False)

    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        stop_evt.set()
        thread.join(timeout=1.0)
        for writer in mode_writers.values():
            writer.close()
        unknown_writer.close()
        cfg_writer.close()
        evt_writer.close()
        ser.close()
        _ = anim

    print(f"[INFO] Closed. Run directory saved: {run_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
