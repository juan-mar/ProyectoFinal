#!/usr/bin/env python3
"""
Offline RSSI CSV post-processing.

Reads CSV file(s) with RSSI samples, recomputes a scalar Kalman filter from raw RSSI,
and generates:
1) Raw RSSI plot
2) Raw vs ESP-cmp vs Kalman(Python) plot

Accepted raw RSSI columns (first match is used):
- rssi_t1
- raw_rssi
- rssi

Accepted ESP comparison columns (optional):
- cmp_t1
- filtered_rssi
- cmp
- kalman_rssi

Optional time axis column:
- millis
If not present, sample index is used.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

import matplotlib.pyplot as plt

from kalman_scalar import ScalarKalmanFilter


def pick_column(fieldnames: Sequence[str], candidates: Sequence[str]) -> Optional[str]:
    lower_map = {name.lower(): name for name in fieldnames}
    for c in candidates:
        if c.lower() in lower_map:
            return lower_map[c.lower()]
    return None


def read_rssi_csv(
    csv_path: Path,
) -> Tuple[List[float], List[float], List[Optional[float]], List[Optional[float]], str]:
    raw_values: List[float] = []
    esp_cmp_values: List[Optional[float]] = []
    py_values: List[Optional[float]] = []
    x_axis: List[float] = []

    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError("CSV has no header row")

        raw_col = pick_column(reader.fieldnames, ["rssi_t1", "raw_rssi", "rssi"])
        if raw_col is None:
            raise ValueError("CSV must contain 'rssi_t1' or 'raw_rssi' or 'rssi' column")

        esp_col = pick_column(reader.fieldnames, ["cmp_t1", "filtered_rssi", "cmp", "kalman_rssi"])
        py_col = pick_column(reader.fieldnames, ["py_t1", "python_kalman"])

        time_col = pick_column(reader.fieldnames, ["millis"])

        for idx, row in enumerate(reader):
            raw_txt = row.get(raw_col, "").strip()
            if not raw_txt:
                continue
            try:
                raw = float(raw_txt)
            except ValueError:
                continue

            if time_col is not None:
                t_txt = row.get(time_col, "").strip()
                try:
                    t_value = float(t_txt)
                except ValueError:
                    t_value = float(idx)
            else:
                t_value = float(idx)

            raw_values.append(raw)
            if esp_col is None:
                esp_cmp_values.append(None)
            else:
                esp_txt = row.get(esp_col, "").strip()
                try:
                    esp_cmp_values.append(float(esp_txt))
                except ValueError:
                    esp_cmp_values.append(None)

            if py_col is None:
                py_values.append(None)
            else:
                py_txt = row.get(py_col, "").strip()
                try:
                    py_values.append(float(py_txt))
                except ValueError:
                    py_values.append(None)
            x_axis.append(t_value)

    if not raw_values:
        raise ValueError("No valid RSSI samples found in CSV")

    x_label = "millis" if time_col is not None else "sample_index"
    return x_axis, raw_values, esp_cmp_values, py_values, x_label


def run_kalman(raw_values: Sequence[float], q: float, r: float, x0: float, p0: float) -> List[float]:
    kf = ScalarKalmanFilter(q=q, r=r, x0=x0, p0=p0)
    filtered: List[float] = []
    for z in raw_values:
        step = kf.step(z)
        filtered.append(step.x_post)
    return filtered


def _find_cfg_for_data_csv(data_csv: Path) -> Optional[Path]:
    name = data_csv.name
    if name.endswith("_train.csv"):
        cfg_name = name[:-10] + "_cfg.csv"
    elif name.endswith("_calib.csv"):
        cfg_name = name[:-10] + "_cfg.csv"
    else:
        return None
    cfg_path = data_csv.with_name(cfg_name)
    return cfg_path if cfg_path.exists() else None


def read_hysteresis_from_cfg(data_csv: Path) -> Tuple[Optional[float], Optional[float]]:
    cfg_path = _find_cfg_for_data_csv(data_csv)
    if cfg_path is None:
        return None, None

    with cfg_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            return None, None

        in_col = pick_column(reader.fieldnames, ["histeresis_in", "hysteresis_in"])
        out_col = pick_column(reader.fieldnames, ["histeresis_out", "hysteresis_out"])
        if in_col is None or out_col is None:
            return None, None

        last_in: Optional[float] = None
        last_out: Optional[float] = None
        for row in reader:
            in_txt = row.get(in_col, "").strip()
            out_txt = row.get(out_col, "").strip()
            try:
                if in_txt:
                    last_in = float(in_txt)
            except ValueError:
                pass
            try:
                if out_txt:
                    last_out = float(out_txt)
            except ValueError:
                pass

        return last_in, last_out


def read_media_calib_from_cfg(data_csv: Path) -> Optional[float]:
    cfg_path = _find_cfg_for_data_csv(data_csv)
    if cfg_path is None:
        return None

    with cfg_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            return None

        media_col = pick_column(reader.fieldnames, ["media_calib", "calib_mean"])
        if media_col is None:
            return None

        last_media: Optional[float] = None
        for row in reader:
            txt = row.get(media_col, "").strip()
            if not txt:
                continue
            try:
                last_media = float(txt)
            except ValueError:
                pass
        return last_media


def estimate_distance_m(
    rssi_value: Optional[float],
    ref_rssi_at_calib: Optional[float],
    calibration_distance_m: float,
    path_loss_exp: float,
) -> Optional[float]:
    if rssi_value is None:
        return None
    if ref_rssi_at_calib is None or calibration_distance_m <= 0.0 or path_loss_exp <= 0.0:
        return None
    exponent = (ref_rssi_at_calib - rssi_value) / (10.0 * path_loss_exp)
    return calibration_distance_m * (10.0 ** exponent)


def build_distance_series(
    source: str,
    raw_values: Sequence[float],
    esp_cmp_values: Sequence[Optional[float]],
    py_values: Sequence[float],
    ref_rssi_at_calib: Optional[float],
    calibration_distance_m: float,
    path_loss_exp: float,
) -> List[Optional[float]]:
    series: List[Optional[float]] = []
    for raw, esp, py in zip(raw_values, esp_cmp_values, py_values):
        if source == "raw":
            rssi_source: Optional[float] = raw
        elif source == "esp":
            rssi_source = esp
        else:
            rssi_source = py
        series.append(
            estimate_distance_m(
                rssi_value=rssi_source,
                ref_rssi_at_calib=ref_rssi_at_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )
        )
    return series


def write_processed_csv(
    out_csv: Path,
    x_axis: Sequence[float],
    x_label: str,
    raw_values: Sequence[float],
    esp_cmp_values: Sequence[Optional[float]],
    py_values: Sequence[float],
    dist_values: Optional[Sequence[Optional[float]]],
) -> None:
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        header = [x_label, "raw_rssi", "esp_cmp", "python_filter"]
        if dist_values is not None:
            header.append("distance_m")
        writer.writerow(header)
        for idx, (x, raw, esp_cmp, py_filt) in enumerate(zip(x_axis, raw_values, esp_cmp_values, py_values)):
            esp_txt = "" if esp_cmp is None else f"{esp_cmp:.6f}"
            row = [f"{x:.6f}", f"{raw:.6f}", esp_txt, f"{py_filt:.6f}"]
            if dist_values is not None:
                dist_v = dist_values[idx]
                row.append("" if dist_v is None or math.isnan(dist_v) else f"{dist_v:.6f}")
            writer.writerow(row)


def plot_raw(
    out_path: Path,
    x_axis: Sequence[float],
    raw_values: Sequence[float],
    x_label: str,
    hysteresis_in: Optional[float],
    hysteresis_out: Optional[float],
) -> None:
    fig, ax = plt.subplots(figsize=(10, 4.5))
    ax.plot(x_axis, raw_values, color="#1f77b4", linewidth=1.3)
    if hysteresis_in is not None:
        ax.axhline(y=hysteresis_in, color="#9467bd", linestyle="--", linewidth=1.2, label="Histeresis IN")
    if hysteresis_out is not None:
        ax.axhline(y=hysteresis_out, color="#8c564b", linestyle=":", linewidth=1.2, label="Histeresis OUT")
    ax.set_title("RSSI sin filtrar")
    ax.set_xlabel(x_label)
    ax.set_ylabel("RSSI (dBm)")
    ax.grid(True, alpha=0.35)
    if hysteresis_in is not None or hysteresis_out is not None:
        ax.legend(loc="best")
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def plot_raw_vs_filtered(
    out_path: Path,
    x_axis: Sequence[float],
    raw_values: Sequence[float],
    esp_cmp_values: Sequence[Optional[float]],
    py_values: Sequence[float],
    x_label: str,
    hysteresis_in: Optional[float],
    hysteresis_out: Optional[float],
) -> None:
    fig, ax = plt.subplots(figsize=(10, 4.5))
    ax.plot(x_axis, raw_values, color="#1f77b4", linewidth=1.1, label="RSSI crudo")
    if any(v is not None for v in esp_cmp_values):
        esp_y = [float("nan") if v is None else v for v in esp_cmp_values]
        ax.plot(x_axis, esp_y, color="#2ca02c", linewidth=1.6, label="CMP ESP")
    ax.plot(x_axis, py_values, color="#d62728", linewidth=1.8, label="Filtro Python")
    if hysteresis_in is not None:
        ax.axhline(y=hysteresis_in, color="#9467bd", linestyle="--", linewidth=1.2, label="Histeresis IN")
    if hysteresis_out is not None:
        ax.axhline(y=hysteresis_out, color="#8c564b", linestyle=":", linewidth=1.2, label="Histeresis OUT")
    ax.set_title("RSSI crudo vs CMP ESP vs Filtro Python")
    ax.set_xlabel(x_label)
    ax.set_ylabel("RSSI (dBm)")
    ax.grid(True, alpha=0.35)
    ax.legend(loc="best")
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def is_generated_processed_csv(path: Path) -> bool:
    return path.name.lower().endswith("_processed.csv")


def discover_input_csvs(input_path: Path) -> List[Path]:
    if input_path.is_file():
        return [input_path]

    if not input_path.is_dir():
        raise ValueError(f"Input path is neither file nor directory: {input_path}")

    # Match data files like: <timestamp>_calib.csv, <timestamp>_train.csv, plus unknown_mode_latest.csv
    run_file_re = re.compile(r".*_(calib|train)\.csv$", re.IGNORECASE)

    candidates: List[Path] = []
    for csv_path in sorted(input_path.rglob("*.csv")):
        if is_generated_processed_csv(csv_path):
            continue
        name = csv_path.name.lower()
        if name.endswith("_cfg.csv") or name.endswith("_evt.csv"):
            continue
        if name == "unknown_mode_latest.csv" or run_file_re.match(name):
            candidates.append(csv_path)

    return candidates


def default_outputs_for_input(input_csv: Path) -> Tuple[Path, Path, Path]:
    out_raw = input_csv.with_name(f"{input_csv.stem}_raw.png")
    out_kalman = input_csv.with_name(f"{input_csv.stem}_raw_vs_kalman.png")
    out_processed = input_csv.with_name(f"{input_csv.stem}_processed.csv")
    return out_raw, out_kalman, out_processed


def process_one_csv(
    input_csv: Path,
    out_raw: Path,
    out_kalman: Path,
    out_processed: Path,
    q: float,
    r: float,
    x0: float,
    p0: float,
    distance_conversion: str,
    distance_source: str,
    calibration_distance_m: float,
    path_loss_exp: float,
) -> int:
    x_axis, raw_values, esp_cmp_values, py_values_in, x_label = read_rssi_csv(input_csv)
    hysteresis_in, hysteresis_out = read_hysteresis_from_cfg(input_csv)
    py_kalman_fallback = run_kalman(
        raw_values=raw_values,
        q=q,
        r=r,
        x0=x0,
        p0=p0,
    )

    # Postprocess must always recompute Python Kalman from raw RSSI,
    # so comparison against firmware cmp_t1 is deterministic and reproducible.
    py_values: List[float] = py_kalman_fallback

    dist_values: Optional[List[Optional[float]]] = None
    if distance_conversion == "on":
        media_calib = read_media_calib_from_cfg(input_csv)
        if media_calib is None:
            print(f"[WARN] {input_csv}: distance conversion requested but media_calib was not found in CFG")
        else:
            dist_values = build_distance_series(
                source=distance_source,
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )

    plot_raw(out_raw, x_axis, raw_values, x_label, hysteresis_in, hysteresis_out)
    plot_raw_vs_filtered(
        out_kalman,
        x_axis,
        raw_values,
        esp_cmp_values,
        py_values,
        x_label,
        hysteresis_in,
        hysteresis_out,
    )
    write_processed_csv(out_processed, x_axis, x_label, raw_values, esp_cmp_values, py_values, dist_values)
    return len(raw_values)


def main() -> int:
    parser = argparse.ArgumentParser(description="Process RSSI CSV file(s) and generate raw + Kalman plots")
    parser.add_argument(
        "--input",
        required=True,
        help="Input CSV file or directory (e.g. scripts/rssi_csv)",
    )
    parser.add_argument("--kalman-q", type=float, default=2.0, help="Kalman process noise Q")
    parser.add_argument("--kalman-r", type=float, default=9.0, help="Kalman measurement noise R")
    parser.add_argument("--kalman-x0", type=float, default=-60.0, help="Kalman initial state x0")
    parser.add_argument("--kalman-p0", type=float, default=100.0, help="Kalman initial covariance p0")
    parser.add_argument(
        "--distance-conversion",
        choices=["off", "on"],
        default="off",
        help="Enable/disable RSSI to distance conversion in processed CSV",
    )
    parser.add_argument(
        "--distance-source",
        choices=["python", "raw", "esp"],
        default="python",
        help="RSSI source used for distance conversion",
    )
    parser.add_argument(
        "--calibration-distance-m",
        type=float,
        default=1.0,
        help="Distance in meters used during calibration to interpret media_calib from CFG",
    )
    parser.add_argument(
        "--path-loss-exp",
        type=float,
        default=2.0,
        help="Path-loss exponent n used in RSSI to distance conversion",
    )
    parser.add_argument("--plot-raw", default="", help="Output PNG for raw RSSI plot")
    parser.add_argument("--plot-kalman", default="", help="Output PNG for raw vs Kalman plot")
    parser.add_argument("--processed-csv", default="", help="Optional output CSV with raw+kalman")
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"[ERROR] Input CSV not found: {input_path}", file=sys.stderr)
        return 1

    try:
        csv_inputs = discover_input_csvs(input_path)
        if not csv_inputs:
            print(f"[ERROR] No matching CSV files found in: {input_path}", file=sys.stderr)
            return 1

        is_single_file = len(csv_inputs) == 1 and csv_inputs[0].is_file() and input_path.is_file()
        if (args.plot_raw or args.plot_kalman or args.processed_csv) and not is_single_file:
            print("[WARN] Custom output paths are only used in single-file mode. Using defaults for batch mode.")

        total_samples = 0
        ok_files = 0
        failed_files = 0

        for csv_file in csv_inputs:
            try:
                default_raw, default_kalman, default_processed = default_outputs_for_input(csv_file)
                out_raw = Path(args.plot_raw) if (args.plot_raw and is_single_file) else default_raw
                out_kalman = Path(args.plot_kalman) if (args.plot_kalman and is_single_file) else default_kalman
                out_processed = Path(args.processed_csv) if (args.processed_csv and is_single_file) else default_processed

                n = process_one_csv(
                    input_csv=csv_file,
                    out_raw=out_raw,
                    out_kalman=out_kalman,
                    out_processed=out_processed,
                    q=args.kalman_q,
                    r=args.kalman_r,
                    x0=args.kalman_x0,
                    p0=args.kalman_p0,
                    distance_conversion=args.distance_conversion,
                    distance_source=args.distance_source,
                    calibration_distance_m=args.calibration_distance_m,
                    path_loss_exp=args.path_loss_exp,
                )
                total_samples += n
                ok_files += 1
                print(f"[OK] {csv_file} -> samples={n}")
                print(f"     raw={out_raw}")
                print(f"     kalman={out_kalman}")
                print(f"     processed={out_processed}")
            except Exception as exc:
                failed_files += 1
                print(f"[ERROR] {csv_file}: {exc}", file=sys.stderr)

        if failed_files > 0 and ok_files == 0:
            return 1

        print(
            "[SUMMARY] "
            f"files_ok={ok_files} files_failed={failed_files} total_samples={total_samples}"
        )
        return 0

    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
