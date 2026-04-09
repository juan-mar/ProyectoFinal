#!/usr/bin/env python3
"""
Offline RSSI CSV post-processing (simple variant).

Generates a single plot per CSV with only:
- RSSI crudo
- Filtro de Kalman (taken from CMP/filtered column in CSV)

No hysteresis lines and no vertical event lines are shown.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

import matplotlib.pyplot as plt


X_MAX_MILLIS_DEFAULT = 1_252_500.0


def pick_column(fieldnames: Sequence[str], candidates: Sequence[str]) -> Optional[str]:
    lower_map = {name.lower(): name for name in fieldnames}
    for c in candidates:
        if c.lower() in lower_map:
            return lower_map[c.lower()]
    return None


def read_rssi_csv(csv_path: Path) -> Tuple[List[float], List[float], List[Optional[float]], str]:
    raw_values: List[float] = []
    kalman_cmp_values: List[Optional[float]] = []
    x_axis: List[float] = []

    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError("CSV has no header row")

        raw_col = pick_column(reader.fieldnames, ["rssi_t1", "raw_rssi", "rssi"])
        if raw_col is None:
            raise ValueError("CSV must contain 'rssi_t1' or 'raw_rssi' or 'rssi' column")

        # Use firmware CMP as the Kalman-filtered signal for this simple plot.
        cmp_col = pick_column(reader.fieldnames, ["cmp_t1", "filtered_rssi", "cmp", "kalman_rssi"])

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
            if cmp_col is None:
                kalman_cmp_values.append(None)
            else:
                cmp_txt = row.get(cmp_col, "").strip()
                try:
                    kalman_cmp_values.append(float(cmp_txt))
                except ValueError:
                    kalman_cmp_values.append(None)
            x_axis.append(t_value)

    if not raw_values:
        raise ValueError("No valid RSSI samples found in CSV")

    x_label = "millis" if time_col is not None else "sample_index"
    return x_axis, raw_values, kalman_cmp_values, x_label


def is_generated_processed_csv(path: Path) -> bool:
    return path.name.lower().endswith("_processed.csv")


def discover_input_csvs(input_path: Path) -> List[Path]:
    if input_path.is_file():
        return [input_path]

    if not input_path.is_dir():
        raise ValueError(f"Input path is neither file nor directory: {input_path}")

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


def default_output_for_input(input_csv: Path) -> Path:
    return input_csv.with_name(f"{input_csv.stem}_raw_vs_kalman_simple.png")


def plot_raw_vs_kalman_simple(
    out_path: Path,
    x_axis: Sequence[float],
    raw_values: Sequence[float],
    kalman_cmp_values: Sequence[Optional[float]],
    x_label: str,
) -> None:
    fig, ax = plt.subplots(figsize=(10, 4.5))

    if x_label == "millis":
        x_plot = [x / 1000.0 for x in x_axis]
        x_label_plot = "Seconds (s)"
        x_max_plot = X_MAX_MILLIS_DEFAULT / 1000.0
    else:
        x_plot = list(x_axis)
        x_label_plot = x_label
        x_max_plot = None

    ax.plot(x_plot, raw_values, color="#1f77b4", linewidth=1.2, label="RSSI crudo")

    if any(v is not None for v in kalman_cmp_values):
        cmp_y = [float("nan") if v is None else v for v in kalman_cmp_values]
        ax.plot(x_plot, cmp_y, color="#2ca02c", linewidth=1.8, label="Filtro de Kalman")

    if x_max_plot is not None:
        ax.set_xlim(right=x_max_plot)

    ax.set_title("RSSI crudo vs Filtro de Kalman")
    ax.set_xlabel(x_label_plot)
    ax.set_ylabel("RSSI (dBm)")
    ax.grid(True, alpha=0.35)
    ax.legend(loc="best")

    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def process_one_csv(input_csv: Path, out_plot: Path) -> int:
    x_axis, raw_values, kalman_cmp_values, x_label = read_rssi_csv(input_csv)
    plot_raw_vs_kalman_simple(
        out_path=out_plot,
        x_axis=x_axis,
        raw_values=raw_values,
        kalman_cmp_values=kalman_cmp_values,
        x_label=x_label,
    )
    return len(raw_values)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Process RSSI CSV file(s) and generate a simple raw vs Kalman(CMP) plot"
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Input CSV file or directory (e.g. scripts/rssi_csv)",
    )
    parser.add_argument(
        "--plot",
        default="",
        help="Output PNG path (single-file mode only)",
    )
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
        if args.plot and not is_single_file:
            print("[WARN] Custom output path is only used in single-file mode. Using defaults for batch mode.")

        total_samples = 0
        ok_files = 0
        failed_files = 0

        for csv_file in csv_inputs:
            try:
                default_plot = default_output_for_input(csv_file)
                out_plot = Path(args.plot) if (args.plot and is_single_file) else default_plot

                n = process_one_csv(input_csv=csv_file, out_plot=out_plot)
                total_samples += n
                ok_files += 1

                print(f"[OK] {csv_file} -> samples={n}")
                print(f"     plot={out_plot}")
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
