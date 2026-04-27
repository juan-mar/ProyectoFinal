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
from ukf_distance import ScalarUnscentedKalmanFilter, distance_to_rssi_log


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


def _find_evt_for_data_csv(data_csv: Path) -> Optional[Path]:
    name = data_csv.name
    if name.endswith("_train.csv"):
        evt_name = name[:-10] + "_evt.csv"
    elif name.endswith("_calib.csv"):
        evt_name = name[:-10] + "_evt.csv"
    else:
        return None
    evt_path = data_csv.with_name(evt_name)
    return evt_path if evt_path.exists() else None


def _find_calib_for_data_csv(data_csv: Path) -> Optional[Path]:
    name = data_csv.name
    if name.endswith("_calib.csv"):
        return data_csv if data_csv.exists() else None
    if name.endswith("_train.csv"):
        calib_name = name[:-10] + "_calib.csv"
        calib_path = data_csv.with_name(calib_name)
        return calib_path if calib_path.exists() else None
    return None


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


def read_evt_from_evt_csv(data_csv: Path) -> List[Tuple[float, str]]:
    evt_path = _find_evt_for_data_csv(data_csv)
    if evt_path is None:
        return []

    events: List[Tuple[float, str]] = []
    with evt_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            return []

        millis_col = pick_column(reader.fieldnames, ["millis"])
        state_col = pick_column(reader.fieldnames, ["state", "state_in_out"])
        if millis_col is None or state_col is None:
            return []

        for row in reader:
            m_txt = row.get(millis_col, "").strip()
            s_txt = row.get(state_col, "").strip().upper()
            if not m_txt or s_txt not in ("IN", "OUT"):
                continue
            try:
                events.append((float(m_txt), s_txt))
            except ValueError:
                continue
    return events


def derive_python_hysteresis_from_calibration(
    data_csv: Path,
    q: float,
    r: float,
    x0: float,
    p0: float,
    source: str,
    k_sigma: float,
) -> Tuple[Optional[float], Optional[float], Optional[float], Optional[float], int, Optional[Path]]:
    """
    Build hysteresis thresholds from calibration statistics.

    Steps:
    - Find matching calibration CSV for the current run.
    - Compute series mean and variance from calibration samples.
    - Derive hysteresis in RSSI domain as:
        in_threshold  = mean + k_sigma * std
        out_threshold = mean - k_sigma * std
    """
    calib_csv = _find_calib_for_data_csv(data_csv)
    if calib_csv is None:
        return None, None, None, None, 0, None

    _, raw_values_cal, _, _, _ = read_rssi_csv(calib_csv)
    if not raw_values_cal:
        return None, None, None, None, 0, calib_csv

    if source == "raw":
        cal_series: List[float] = [float(v) for v in raw_values_cal]
    else:
        cal_series = run_kalman(raw_values=raw_values_cal, q=q, r=r, x0=x0, p0=p0)

    n = len(cal_series)
    if n == 0:
        return None, None, None, None, 0, calib_csv

    mean_cal = sum(cal_series) / float(n)
    var_cal = sum((v - mean_cal) ** 2 for v in cal_series) / float(n)
    std_cal = math.sqrt(var_cal)

    hysteresis_in = mean_cal + (k_sigma * std_cal)
    hysteresis_out = mean_cal - (k_sigma * std_cal)
    return hysteresis_in, hysteresis_out, mean_cal, var_cal, n, calib_csv


def derive_python_two_stage_calibration(
    data_csv: Path,
    q: float,
    x0: float,
    p0: float,
    k_sigma: float,
    split_ratio: float,
) -> Tuple[
    Optional[float],
    Optional[float],
    Optional[float],
    Optional[float],
    Optional[float],
    Optional[float],
    int,
    int,
    Optional[Path],
]:
    """Two-stage calibration from the same CAL CSV.

    Stage A (raw): estimate measurement noise and x0 from raw RSSI.
      - R_two_stage = var(raw_stage_a)
      - x0_two_stage = mean(raw_stage_a)

    Stage B (cmp/python): run Kalman over raw_stage_b using (q, R_two_stage, x0_two_stage, p0),
    then derive hysteresis over filtered series:
      - in_threshold  = mean(filtered_stage_b) + k_sigma * std(filtered_stage_b)
      - out_threshold = mean(filtered_stage_b) - k_sigma * std(filtered_stage_b)
    """
    calib_csv = _find_calib_for_data_csv(data_csv)
    if calib_csv is None:
        return None, None, None, None, None, None, 0, 0, None

    _, raw_values_cal, _, _, _ = read_rssi_csv(calib_csv)
    n_total = len(raw_values_cal)
    if n_total < 6:
        return None, None, None, None, None, None, 0, 0, calib_csv

    bounded_ratio = min(max(split_ratio, 0.2), 0.8)
    n_a = int(round(n_total * bounded_ratio))
    n_a = min(max(n_a, 3), n_total - 3)
    n_b = n_total - n_a

    stage_a_raw = [float(v) for v in raw_values_cal[:n_a]]
    stage_b_raw = [float(v) for v in raw_values_cal[n_a:]]

    mean_raw_a = sum(stage_a_raw) / float(n_a)
    var_raw_a = sum((v - mean_raw_a) ** 2 for v in stage_a_raw) / float(n_a)
    r_two_stage = max(var_raw_a, 1e-6)
    x0_two_stage = mean_raw_a

    filtered_stage_b = run_kalman(
        raw_values=stage_b_raw,
        q=q,
        r=r_two_stage,
        x0=x0_two_stage,
        p0=p0,
    )
    if not filtered_stage_b:
        return None, None, None, None, None, None, n_a, n_b, calib_csv

    mean_cmp_b = sum(filtered_stage_b) / float(len(filtered_stage_b))
    var_cmp_b = sum((v - mean_cmp_b) ** 2 for v in filtered_stage_b) / float(len(filtered_stage_b))
    std_cmp_b = math.sqrt(var_cmp_b)

    hysteresis_in = mean_cmp_b + (k_sigma * std_cmp_b)
    hysteresis_out = mean_cmp_b - (k_sigma * std_cmp_b)

    return (
        hysteresis_in,
        hysteresis_out,
        r_two_stage,
        x0_two_stage,
        mean_cmp_b,
        var_cmp_b,
        n_a,
        n_b,
        calib_csv,
    )


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


def estimate_relative_gamma(
    rssi_value: Optional[float],
    ref_rssi_at_calib: Optional[float],
    path_loss_exp: float,
) -> Optional[float]:
    """Gamma relative to calibration point: gamma = log10(d/d0)."""
    if rssi_value is None:
        return None
    if ref_rssi_at_calib is None or path_loss_exp <= 0.0:
        return None
    return (ref_rssi_at_calib - rssi_value) / (10.0 * path_loss_exp)


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


def build_gamma_series(
    source: str,
    raw_values: Sequence[float],
    esp_cmp_values: Sequence[Optional[float]],
    py_values: Sequence[float],
    ref_rssi_at_calib: Optional[float],
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
            estimate_relative_gamma(
                rssi_value=rssi_source,
                ref_rssi_at_calib=ref_rssi_at_calib,
                path_loss_exp=path_loss_exp,
            )
        )
    return series


def infer_state_changes_from_distance(
    x_axis: Sequence[float],
    dist_values: Sequence[Optional[float]],
    dist_in_threshold: Optional[float],
    dist_out_threshold: Optional[float],
    initial_state: str = "OUT",
) -> List[Tuple[float, str]]:
    """Infer IN/OUT transitions using hysteresis in distance domain.

    Distance hysteresis logic:
    - Switch to IN when distance <= in_threshold
    - Switch to OUT when distance >= out_threshold
    - Between thresholds keep previous state
    """
    if dist_in_threshold is None or dist_out_threshold is None:
        return []

    in_th = float(min(dist_in_threshold, dist_out_threshold))
    out_th = float(max(dist_in_threshold, dist_out_threshold))
    state = initial_state if initial_state in ("IN", "OUT") else "OUT"
    changes: List[Tuple[float, str]] = []

    for x, d in zip(x_axis, dist_values):
        if d is None:
            continue
        if state != "IN" and d <= in_th:
            state = "IN"
            changes.append((float(x), "IN"))
        elif state != "OUT" and d >= out_th:
            state = "OUT"
            changes.append((float(x), "OUT"))
    return changes


def infer_state_changes_from_rssi(
    x_axis: Sequence[float],
    rssi_values: Sequence[Optional[float]],
    rssi_in_threshold: Optional[float],
    rssi_out_threshold: Optional[float],
    initial_state: str = "OUT",
) -> List[Tuple[float, str]]:
    """Infer IN/OUT transitions using hysteresis in RSSI domain.

    RSSI hysteresis logic:
    - Switch to IN when RSSI >= in_threshold (signal stronger = closer = IN)
    - Switch to OUT when RSSI <= out_threshold (signal weaker = farther = OUT)
    - Between thresholds keep previous state
    """
    if rssi_in_threshold is None or rssi_out_threshold is None:
        return []

    in_th = float(max(rssi_in_threshold, rssi_out_threshold))  # stronger signal
    out_th = float(min(rssi_in_threshold, rssi_out_threshold))  # weaker signal
    state = initial_state if initial_state in ("IN", "OUT") else "OUT"
    changes: List[Tuple[float, str]] = []

    for x, rssi in zip(x_axis, rssi_values):
        if rssi is None:
            continue
        if state != "IN" and rssi >= in_th:
            state = "IN"
            changes.append((float(x), "IN"))
        elif state != "OUT" and rssi <= out_th:
            state = "OUT"
            changes.append((float(x), "OUT"))
    return changes


def run_distance_ukf_series(
    rssi_values: Sequence[Optional[float]],
    ref_rssi_at_calib: Optional[float],
    calibration_distance_m: float,
    path_loss_exp: float,
    ukf_q: float,
    ukf_r: float,
    ukf_alpha: float,
    ukf_beta: float,
    ukf_kappa: float,
    ukf_init_m: float,
    ukf_init_p: float,
) -> List[Optional[float]]:
    """
    Runs scalar UKF with:
    - state x := distance (m)
    - measurement z := RSSI (dBm)

    Process model f(x) used here: identity (random walk).
    Measurement model h(x) used here: log-distance path loss.
    """
    if ref_rssi_at_calib is None:
        return [None for _ in rssi_values]

    ukf = ScalarUnscentedKalmanFilter(
        alpha=ukf_alpha,
        beta=ukf_beta,
        kappa=ukf_kappa,
        x0=ukf_init_m,
        p0=ukf_init_p,
    )

    out: List[Optional[float]] = []
    for z in rssi_values:
        if z is None:
            out.append(None)
            continue
        step = ukf.step(
            z=float(z),
            f=lambda x: x,
            h=lambda x: distance_to_rssi_log(
                distance_m=x,
                ref_rssi_at_calib=ref_rssi_at_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            ),
            q=ukf_q,
            r=ukf_r,
        )
        out.append(step.x_post)
    return out


def run_gamma_ukf_series(
    rssi_values: Sequence[Optional[float]],
    ref_rssi_at_calib: Optional[float],
    path_loss_exp: float,
    ukf_q: float,
    ukf_r: float,
    ukf_alpha: float,
    ukf_beta: float,
    ukf_kappa: float,
    ukf_init_gamma: float,
    ukf_init_p: float,
) -> List[Optional[float]]:
    """
    Runs scalar UKF with:
    - state x := gamma = log10(d/d0)
    - measurement z := RSSI (dBm)

    Measurement model:
      z = ref_rssi_at_calib - 10*n*gamma
    """
    if ref_rssi_at_calib is None or path_loss_exp <= 0.0:
        return [None for _ in rssi_values]

    ukf = ScalarUnscentedKalmanFilter(
        alpha=ukf_alpha,
        beta=ukf_beta,
        kappa=ukf_kappa,
        x0=ukf_init_gamma,
        p0=ukf_init_p,
    )

    out: List[Optional[float]] = []
    for z in rssi_values:
        if z is None:
            out.append(None)
            continue
        step = ukf.step(
            z=float(z),
            f=lambda x: x,
            h=lambda x: ref_rssi_at_calib - (10.0 * path_loss_exp * x),
            q=ukf_q,
            r=ukf_r,
        )
        out.append(step.x_post)
    return out


def write_processed_csv(
    out_csv: Path,
    x_axis: Sequence[float],
    x_label: str,
    raw_values: Sequence[float],
    esp_cmp_values: Sequence[Optional[float]],
    py_values: Sequence[float],
    dist_values: Optional[Sequence[Optional[float]]],
    dist_ukf_values: Optional[Sequence[Optional[float]]],
) -> None:
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        header = [x_label, "raw_rssi", "esp_cmp", "python_filter"]
        if dist_values is not None:
            header.append("distance_m")
        if dist_ukf_values is not None:
            header.append("distance_ukf_m")
        writer.writerow(header)
        for idx, (x, raw, esp_cmp, py_filt) in enumerate(zip(x_axis, raw_values, esp_cmp_values, py_values)):
            esp_txt = "" if esp_cmp is None else f"{esp_cmp:.6f}"
            row = [f"{x:.6f}", f"{raw:.6f}", esp_txt, f"{py_filt:.6f}"]
            if dist_values is not None:
                dist_v = dist_values[idx]
                row.append("" if dist_v is None or math.isnan(dist_v) else f"{dist_v:.6f}")
            if dist_ukf_values is not None:
                dist_ukf_v = dist_ukf_values[idx]
                row.append("" if dist_ukf_v is None or math.isnan(dist_ukf_v) else f"{dist_ukf_v:.6f}")
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
    fw_hysteresis_in: Optional[float],
    fw_hysteresis_out: Optional[float],
    py_hysteresis_in: Optional[float],
    py_hysteresis_out: Optional[float],
    inferred_changes: Sequence[Tuple[float, str]] = [],
    inferred_py_changes: Sequence[Tuple[float, str]] = [],
    firmware_events: Sequence[Tuple[float, str]] = [],
    plot_mode: str = "full",
    show_firmware_events: bool = False,
) -> None:
    fig, ax = plt.subplots(figsize=(10, 4.5))
    ax.plot(x_axis, raw_values, color="#1f77b4", linewidth=1.1, label="RSSI crudo")
    if any(v is not None for v in esp_cmp_values):
        esp_y = [float("nan") if v is None else v for v in esp_cmp_values]
        ax.plot(x_axis, esp_y, color="#ef0808", linewidth=1.6, label="RSSI Filtrado")
    if plot_mode != "fw-only":
        ax.plot(x_axis, py_values, color="#d62728", linewidth=1.8, label="Filtro Python")
    if fw_hysteresis_in is not None:
        ax.axhline(
            y=fw_hysteresis_in,
            color="#2ca02c",
            linestyle="--",
            linewidth=1.1,
            alpha=0.9,
            label="Umbral IN",
        )
    if fw_hysteresis_out is not None:
        ax.axhline(
            y=fw_hysteresis_out,
            color="#2ca02c",
            linestyle=":",
            linewidth=1.1,
            alpha=0.9,
            label="Umbral OUT",
        )
    if plot_mode != "fw-only" and py_hysteresis_in is not None:
        ax.axhline(
            y=py_hysteresis_in,
            color="#d62728",
            linestyle="--",
            linewidth=1.3,
            alpha=0.9,
            label="Histeresis PY IN (two-stage)",
        )
    if plot_mode != "fw-only" and py_hysteresis_out is not None:
        ax.axhline(
            y=py_hysteresis_out,
            color="#d62728",
            linestyle=":",
            linewidth=1.3,
            alpha=0.9,
            label="Histeresis PY OUT (two-stage)",
        )
    cmp_in_labeled = False
    cmp_out_labeled = False
    py_in_labeled = False
    py_out_labeled = False
    fw_evt_labeled = False

    for x_evt, st in inferred_changes:
        color = "#ff7f0e" if st == "IN" else "#9467bd"
        lbl = None
        if st == "IN" and not cmp_in_labeled:
            lbl = "Evento IN"
            cmp_in_labeled = True
        elif st == "OUT" and not cmp_out_labeled:
            lbl = "Evento OUT"
            cmp_out_labeled = True
        ax.axvline(x=x_evt, color=color, linestyle="-", linewidth=0.9, alpha=0.6, label=lbl)

    if plot_mode != "fw-only":
        for x_evt, st in inferred_py_changes:
            color = "#17becf" if st == "IN" else "#e377c2"
            lbl = None
            if st == "IN" and not py_in_labeled:
                lbl = "Cambio IN (Python+Hyst PY)"
                py_in_labeled = True
            elif st == "OUT" and not py_out_labeled:
                lbl = "Cambio OUT (Python+Hyst PY)"
                py_out_labeled = True
            ax.axvline(x=x_evt, color=color, linestyle="-.", linewidth=1.0, alpha=0.7, label=lbl)

    if show_firmware_events:
        for x_evt, st in firmware_events:
            color = "#2f2f2f" if st == "IN" else "#666666"
            lbl = None
            if not fw_evt_labeled:
                lbl = "EVT firmware"
                fw_evt_labeled = True
            ax.axvline(x=x_evt, color=color, linestyle="--", linewidth=0.8, alpha=0.45, label=lbl)
    if plot_mode == "fw-only":
        ax.set_title("")
    else:
        ax.set_title("RSSI crudo vs CMP ESP vs Filtro Python")
    ax.set_xlabel(x_label)
    ax.set_ylabel("RSSI (dBm)")
    ax.grid(True, alpha=0.35)
    ax.legend(loc="best")
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def plot_distance(
    out_path: Path,
    x_axis: Sequence[float],
    dist_raw: Sequence[Optional[float]],
    dist_esp: Sequence[Optional[float]],
    dist_py: Sequence[Optional[float]],
    dist_ukf: Sequence[Optional[float]],
    x_label: str,
    distance_source: str,
    hysteresis_in_dist: Optional[float],
    hysteresis_out_dist: Optional[float],
    inferred_changes: Sequence[Tuple[float, str]],
    firmware_events: Sequence[Tuple[float, str]],
    show_firmware_events: bool = False,
) -> None:
    fig, ax = plt.subplots(figsize=(10, 4.5))
    
    has_raw = any(v is not None for v in dist_raw)
    has_esp = any(v is not None for v in dist_esp)
    has_py = any(v is not None for v in dist_py)
    has_ukf = any(v is not None for v in dist_ukf)
    
    if has_raw:
        raw_y = [float("nan") if v is None else v for v in dist_raw]
        ax.plot(x_axis, raw_y, color="#1f77b4", linewidth=1.1, label="Distancia desde RSSI crudo")
    
    if has_esp:
        esp_y = [float("nan") if v is None else v for v in dist_esp]
        ax.plot(x_axis, esp_y, color="#2ca02c", linewidth=1.6, label="Distancia desde CMP ESP")
    
    if has_py:
        py_y = [float("nan") if v is None else v for v in dist_py]
        ax.plot(x_axis, py_y, color="#d62728", linewidth=1.8, label="Distancia desde Filtro Python")

    if has_ukf:
        ukf_y = [float("nan") if v is None else v for v in dist_ukf]
        ax.plot(x_axis, ukf_y, color="#ff7f0e", linewidth=2.0, label="Distancia UKF")

    if hysteresis_in_dist is not None:
        ax.axhline(
            y=hysteresis_in_dist,
            color="#9467bd",
            linestyle="--",
            linewidth=1.2,
            label="Histeresis IN (dist)",
        )
    if hysteresis_out_dist is not None:
        ax.axhline(
            y=hysteresis_out_dist,
            color="#8c564b",
            linestyle=":",
            linewidth=1.2,
            label="Histeresis OUT (dist)",
        )

    # Predicted transitions from Python distance hysteresis.
    for x_evt, st in inferred_changes:
        color = "#ff7f0e" if st == "IN" else "#9467bd"
        ax.axvline(x=x_evt, color=color, linestyle="-", linewidth=0.9, alpha=0.6)

    # Optional firmware EVT transitions for visual comparison.
    if show_firmware_events:
        for x_evt, st in firmware_events:
            color = "#2f2f2f" if st == "IN" else "#666666"
            ax.axvline(x=x_evt, color=color, linestyle="--", linewidth=0.8, alpha=0.45)
    
    ax.set_title(f"Estimación de distancia (fuente: {distance_source})")
    ax.set_xlabel(x_label)
    ax.set_ylabel("Distancia (m)")
    ax.grid(True, alpha=0.35)
    if has_raw or has_esp or has_py or has_ukf:
        ax.legend(loc="best")
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def plot_gamma(
    out_path: Path,
    x_axis: Sequence[float],
    gamma_raw: Sequence[Optional[float]],
    gamma_esp: Sequence[Optional[float]],
    gamma_py: Sequence[Optional[float]],
    gamma_ukf: Sequence[Optional[float]],
    x_label: str,
    gamma_source: str,
    hysteresis_in_gamma: Optional[float],
    hysteresis_out_gamma: Optional[float],
    inferred_changes: Sequence[Tuple[float, str]],
    firmware_events: Sequence[Tuple[float, str]],
    show_firmware_events: bool = False,
) -> None:
    fig, ax = plt.subplots(figsize=(10, 4.5))

    has_raw = any(v is not None for v in gamma_raw)
    has_esp = any(v is not None for v in gamma_esp)
    has_py = any(v is not None for v in gamma_py)
    has_ukf = any(v is not None for v in gamma_ukf)

    if has_raw:
        raw_y = [float("nan") if v is None else v for v in gamma_raw]
        ax.plot(x_axis, raw_y, color="#1f77b4", linewidth=1.1, label="Gamma desde RSSI crudo")

    if has_esp:
        esp_y = [float("nan") if v is None else v for v in gamma_esp]
        ax.plot(x_axis, esp_y, color="#2ca02c", linewidth=1.6, label="Gamma desde CMP ESP")

    if has_py:
        py_y = [float("nan") if v is None else v for v in gamma_py]
        ax.plot(x_axis, py_y, color="#d62728", linewidth=1.8, label="Gamma desde Filtro Python")

    if has_ukf:
        ukf_y = [float("nan") if v is None else v for v in gamma_ukf]
        ax.plot(x_axis, ukf_y, color="#ff7f0e", linewidth=2.0, label="Gamma UKF")

    if hysteresis_in_gamma is not None:
        ax.axhline(
            y=hysteresis_in_gamma,
            color="#9467bd",
            linestyle="--",
            linewidth=1.2,
            label="Histeresis IN (gamma)",
        )
    if hysteresis_out_gamma is not None:
        ax.axhline(
            y=hysteresis_out_gamma,
            color="#8c564b",
            linestyle=":",
            linewidth=1.2,
            label="Histeresis OUT (gamma)",
        )

    for x_evt, st in inferred_changes:
        color = "#ff7f0e" if st == "IN" else "#9467bd"
        ax.axvline(x=x_evt, color=color, linestyle="-", linewidth=0.9, alpha=0.6)

    if show_firmware_events:
        for x_evt, st in firmware_events:
            color = "#2f2f2f" if st == "IN" else "#666666"
            ax.axvline(x=x_evt, color=color, linestyle="--", linewidth=0.8, alpha=0.45)

    ax.set_title(f"Comparación relativa gamma (fuente: {gamma_source})")
    ax.set_xlabel(x_label)
    ax.set_ylabel("Gamma = log10(d/d0)")
    ax.grid(True, alpha=0.35)
    if has_raw or has_esp or has_py or has_ukf:
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


def default_outputs_for_input(input_csv: Path) -> Tuple[Path, Path, Path, Path, Path]:
    out_raw = input_csv.with_name(f"{input_csv.stem}_raw.png")
    out_kalman = input_csv.with_name(f"{input_csv.stem}_raw_vs_kalman.png")
    out_processed = input_csv.with_name(f"{input_csv.stem}_processed.csv")
    out_distance = input_csv.with_name(f"{input_csv.stem}_distance.png")
    out_gamma = input_csv.with_name(f"{input_csv.stem}_gamma.png")
    return out_raw, out_kalman, out_processed, out_distance, out_gamma


def process_one_csv(
    input_csv: Path,
    out_raw: Path,
    out_kalman: Path,
    out_processed: Path,
    out_distance: Path,
    out_gamma: Path,
    q: float,
    r: float,
    x0: float,
    p0: float,
    distance_conversion: str,
    distance_source: str,
    calibration_distance_m: float,
    path_loss_exp: float,
    distance_ukf: str,
    distance_ukf_source: str,
    distance_ukf_q: float,
    distance_ukf_r: float,
    distance_ukf_alpha: float,
    distance_ukf_beta: float,
    distance_ukf_kappa: float,
    distance_ukf_init_m: float,
    distance_ukf_init_p: float,
    hysteresis_source: str,
    py_hysteresis_source: str,
    py_hysteresis_k_sigma: float,
    python_calibration_mode: str,
    python_two_stage_split_ratio: float,
    python_two_stage_k_sigma: float,
    hysteresis_width: Optional[float],
    gamma_comparison: str,
    gamma_source: str,
    gamma_ukf: str,
    gamma_ukf_source: str,
    gamma_ukf_q: float,
    gamma_ukf_r: float,
    gamma_ukf_alpha: float,
    gamma_ukf_beta: float,
    gamma_ukf_kappa: float,
    gamma_ukf_init: float,
    gamma_ukf_init_p: float,
    plot_mode: str,
    show_firmware_events: bool,
) -> int:
    x_axis, raw_values, esp_cmp_values, py_values_in, x_label = read_rssi_csv(input_csv)
    fw_hysteresis_in, fw_hysteresis_out = read_hysteresis_from_cfg(input_csv)
    effective_q = q
    effective_r = r
    effective_x0 = x0
    effective_p0 = p0

    py_hysteresis_in: Optional[float] = None
    py_hysteresis_out: Optional[float] = None
    hysteresis_in = fw_hysteresis_in
    hysteresis_out = fw_hysteresis_out

    if hysteresis_width is not None:
        media_calib_for_hyst = read_media_calib_from_cfg(input_csv)
        if media_calib_for_hyst is not None:
            fw_hysteresis_in = media_calib_for_hyst + hysteresis_width
            fw_hysteresis_out = media_calib_for_hyst - hysteresis_width
            hysteresis_in = fw_hysteresis_in
            hysteresis_out = fw_hysteresis_out
            print(
                f"[HYST-WIDTH] {input_csv.name}: width={hysteresis_width} "
                f"media_calib={media_calib_for_hyst:.4f} "
                f"fw_in={fw_hysteresis_in:.4f} fw_out={fw_hysteresis_out:.4f}"
            )

    if python_calibration_mode == "two-stage" or hysteresis_source == "python-two-stage":
        (
            ts_hysteresis_in,
            ts_hysteresis_out,
            ts_r,
            ts_x0,
            ts_mean_cmp,
            ts_var_cmp,
            ts_n_a,
            ts_n_b,
            ts_calib_csv,
        ) = derive_python_two_stage_calibration(
            data_csv=input_csv,
            q=q,
            x0=x0,
            p0=p0,
            k_sigma=python_two_stage_k_sigma,
            split_ratio=python_two_stage_split_ratio,
        )
        if (
            ts_hysteresis_in is not None
            and ts_hysteresis_out is not None
            and ts_r is not None
            and ts_x0 is not None
        ):
            effective_r = ts_r
            effective_x0 = ts_x0
            py_hysteresis_in = ts_hysteresis_in
            py_hysteresis_out = ts_hysteresis_out
            print(
                f"[CAL-2STAGE] {input_csv.name}: cal_file={ts_calib_csv.name if ts_calib_csv is not None else 'N/A'} "
                f"A={ts_n_a} B={ts_n_b} R={effective_r:.4f} x0={effective_x0:.4f} "
                f"mean_cmp={ts_mean_cmp:.4f} var_cmp={ts_var_cmp:.4f} "
                f"py_in={py_hysteresis_in:.4f} py_out={py_hysteresis_out:.4f}"
            )
            if hysteresis_source == "python-two-stage":
                hysteresis_in = py_hysteresis_in
                hysteresis_out = py_hysteresis_out
        else:
            print(
                f"[WARN] {input_csv.name}: two-stage calibration requested but could not be derived. "
                "Using CLI Kalman params and requested hysteresis fallback."
            )

    if hysteresis_source == "python-cal":
        (
            py_hysteresis_in,
            py_hysteresis_out,
            cal_mean,
            cal_var,
            cal_n,
            cal_csv,
        ) = derive_python_hysteresis_from_calibration(
            data_csv=input_csv,
            q=q,
            r=r,
            x0=x0,
            p0=p0,
            source=py_hysteresis_source,
            k_sigma=py_hysteresis_k_sigma,
        )
        if py_hysteresis_in is not None and py_hysteresis_out is not None:
            hysteresis_in = py_hysteresis_in
            hysteresis_out = py_hysteresis_out
            print(
                f"[HYST-PY] {input_csv.name}: source={py_hysteresis_source} "
                f"cal_file={cal_csv.name if cal_csv is not None else 'N/A'} "
                f"N={cal_n} mean={cal_mean:.4f} var={cal_var:.4f} "
                f"in={hysteresis_in:.4f} out={hysteresis_out:.4f} "
                f"k_sigma={py_hysteresis_k_sigma}"
            )
        else:
            print(
                f"[WARN] {input_csv.name}: python-cal hysteresis requested but calibration statistics "
                "could not be derived. Falling back to firmware CFG hysteresis."
            )
    firmware_events = read_evt_from_evt_csv(input_csv) if show_firmware_events else []
    py_kalman_fallback = run_kalman(
        raw_values=raw_values,
        q=effective_q,
        r=effective_r,
        x0=effective_x0,
        p0=effective_p0,
    )

    # Postprocess must always recompute Python Kalman from raw RSSI,
    # so comparison against firmware cmp_t1 is deterministic and reproducible.
    py_values: List[float] = py_kalman_fallback
    media_calib = read_media_calib_from_cfg(input_csv)
    firmware_events_for_plot = firmware_events if x_label == "millis" else []

    inferred_rssi_changes = infer_state_changes_from_rssi(
        x_axis=x_axis,
        rssi_values=esp_cmp_values,
        rssi_in_threshold=fw_hysteresis_in,
        rssi_out_threshold=fw_hysteresis_out,
        initial_state="OUT",
    )

    py_decision_hyst_in = py_hysteresis_in if py_hysteresis_in is not None else hysteresis_in
    py_decision_hyst_out = py_hysteresis_out if py_hysteresis_out is not None else hysteresis_out
    inferred_python_rssi_changes = infer_state_changes_from_rssi(
        x_axis=x_axis,
        rssi_values=py_values,
        rssi_in_threshold=py_decision_hyst_in,
        rssi_out_threshold=py_decision_hyst_out,
        initial_state="OUT",
    )

    dist_values: Optional[List[Optional[float]]] = None
    dist_ukf_values: Optional[List[Optional[float]]] = None
    if distance_conversion == "on":
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
            # Generate third curve for distance from each RSSI source
            dist_raw_series = build_distance_series(
                source="raw",
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )
            dist_esp_series = build_distance_series(
                source="esp",
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )
            dist_py_series = build_distance_series(
                source="python",
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )

            hysteresis_in_dist = estimate_distance_m(
                rssi_value=hysteresis_in,
                ref_rssi_at_calib=media_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )
            hysteresis_out_dist = estimate_distance_m(
                rssi_value=hysteresis_out,
                ref_rssi_at_calib=media_calib,
                calibration_distance_m=calibration_distance_m,
                path_loss_exp=path_loss_exp,
            )

            # Inferred IN/OUT based on Python distance hysteresis (for comparison).
            inferred_changes = infer_state_changes_from_distance(
                x_axis=x_axis,
                dist_values=dist_py_series,
                dist_in_threshold=hysteresis_in_dist,
                dist_out_threshold=hysteresis_out_dist,
                initial_state="OUT",
            )
            print(
                f"[INFO] {input_csv.name}: inferred_distance_transitions={len(inferred_changes)} "
                f"firmware_evt_for_plot={len(firmware_events_for_plot)}"
            )
            if distance_ukf == "on":
                if distance_ukf_source == "raw":
                    rssi_for_ukf: List[Optional[float]] = [float(v) for v in raw_values]
                elif distance_ukf_source == "esp":
                    rssi_for_ukf = list(esp_cmp_values)
                else:
                    rssi_for_ukf = [float(v) for v in py_values]

                dist_ukf_values = run_distance_ukf_series(
                    rssi_values=rssi_for_ukf,
                    ref_rssi_at_calib=media_calib,
                    calibration_distance_m=calibration_distance_m,
                    path_loss_exp=path_loss_exp,
                    ukf_q=distance_ukf_q,
                    ukf_r=distance_ukf_r,
                    ukf_alpha=distance_ukf_alpha,
                    ukf_beta=distance_ukf_beta,
                    ukf_kappa=distance_ukf_kappa,
                    ukf_init_m=distance_ukf_init_m,
                    ukf_init_p=distance_ukf_init_p,
                )
            plot_distance(
                out_distance,
                x_axis,
                dist_raw_series,
                dist_esp_series,
                dist_py_series,
                dist_ukf_values if dist_ukf_values is not None else [None for _ in x_axis],
                x_label,
                distance_source,
                hysteresis_in_dist,
                hysteresis_out_dist,
                inferred_changes,
                firmware_events_for_plot,
                show_firmware_events=show_firmware_events,
            )

    if gamma_comparison == "on":
        if media_calib is None:
            print(f"[WARN] {input_csv}: gamma comparison requested but media_calib was not found in CFG")
        else:
            gamma_raw_series = build_gamma_series(
                source="raw",
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                path_loss_exp=path_loss_exp,
            )
            gamma_esp_series = build_gamma_series(
                source="esp",
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                path_loss_exp=path_loss_exp,
            )
            gamma_py_series = build_gamma_series(
                source="python",
                raw_values=raw_values,
                esp_cmp_values=esp_cmp_values,
                py_values=py_values,
                ref_rssi_at_calib=media_calib,
                path_loss_exp=path_loss_exp,
            )
            hysteresis_in_gamma = estimate_relative_gamma(
                rssi_value=hysteresis_in,
                ref_rssi_at_calib=media_calib,
                path_loss_exp=path_loss_exp,
            )
            hysteresis_out_gamma = estimate_relative_gamma(
                rssi_value=hysteresis_out,
                ref_rssi_at_calib=media_calib,
                path_loss_exp=path_loss_exp,
            )
            inferred_gamma_changes = infer_state_changes_from_distance(
                x_axis=x_axis,
                dist_values=gamma_py_series,
                dist_in_threshold=hysteresis_in_gamma,
                dist_out_threshold=hysteresis_out_gamma,
                initial_state="OUT",
            )
            gamma_ukf_values: List[Optional[float]] = [None for _ in x_axis]
            if gamma_ukf == "on":
                if gamma_ukf_source == "raw":
                    rssi_for_gamma_ukf: List[Optional[float]] = [float(v) for v in raw_values]
                elif gamma_ukf_source == "esp":
                    rssi_for_gamma_ukf = list(esp_cmp_values)
                else:
                    rssi_for_gamma_ukf = [float(v) for v in py_values]

                gamma_ukf_values = run_gamma_ukf_series(
                    rssi_values=rssi_for_gamma_ukf,
                    ref_rssi_at_calib=media_calib,
                    path_loss_exp=path_loss_exp,
                    ukf_q=gamma_ukf_q,
                    ukf_r=gamma_ukf_r,
                    ukf_alpha=gamma_ukf_alpha,
                    ukf_beta=gamma_ukf_beta,
                    ukf_kappa=gamma_ukf_kappa,
                    ukf_init_gamma=gamma_ukf_init,
                    ukf_init_p=gamma_ukf_init_p,
                )

            plot_gamma(
                out_path=out_gamma,
                x_axis=x_axis,
                gamma_raw=gamma_raw_series,
                gamma_esp=gamma_esp_series,
                gamma_py=gamma_py_series,
                gamma_ukf=gamma_ukf_values,
                x_label=x_label,
                gamma_source=gamma_source,
                hysteresis_in_gamma=hysteresis_in_gamma,
                hysteresis_out_gamma=hysteresis_out_gamma,
                inferred_changes=inferred_gamma_changes,
                firmware_events=firmware_events_for_plot,
                show_firmware_events=show_firmware_events,
            )

    plot_raw(out_raw, x_axis, raw_values, x_label, hysteresis_in, hysteresis_out)
    plot_raw_vs_filtered(
        out_kalman,
        x_axis,
        raw_values,
        esp_cmp_values,
        py_values,
        x_label,
        fw_hysteresis_in,
        fw_hysteresis_out,
        py_hysteresis_in,
        py_hysteresis_out,
        inferred_changes=inferred_rssi_changes,
        inferred_py_changes=inferred_python_rssi_changes,
        firmware_events=firmware_events_for_plot,
        plot_mode=plot_mode,
        show_firmware_events=show_firmware_events,
    )
    write_processed_csv(
        out_processed,
        x_axis,
        x_label,
        raw_values,
        esp_cmp_values,
        py_values,
        dist_values,
        dist_ukf_values,
    )
    return len(raw_values)


def main() -> int:
    parser = argparse.ArgumentParser(description="Process RSSI CSV file(s) and generate raw + Kalman plots")
    parser.add_argument(
        "--input",
        required=True,
        help="Input CSV file or directory (e.g. scripts/rssi_csv)",
    )
    parser.add_argument(
        "--kalman-q",
        "--linear-kf-q",
        dest="kalman_q",
        type=float,
        default=2.0,
        help="Linear Kalman process noise Q",
    )
    parser.add_argument(
        "--kalman-r",
        "--linear-kf-r",
        dest="kalman_r",
        type=float,
        default=9.0,
        help="Linear Kalman measurement noise R",
    )
    parser.add_argument(
        "--kalman-x0",
        "--linear-kf-x0",
        dest="kalman_x0",
        type=float,
        default=-60.0,
        help="Linear Kalman initial state x0",
    )
    parser.add_argument(
        "--kalman-p0",
        "--linear-kf-p0",
        dest="kalman_p0",
        type=float,
        default=100.0,
        help="Linear Kalman initial covariance P0",
    )
    parser.add_argument(
        "--linear-kf-print-config",
        action="store_true",
        help="Print effective linear Kalman parameters before processing",
    )
    parser.add_argument(
        "--distance-conversion",
        choices=["off", "on"],
        default="off",
        help="Enable/disable RSSI to distance conversion in processed CSV",
    )
    parser.add_argument(
        "--hysteresis-source",
        choices=["firmware", "python-cal", "python-two-stage"],
        default="firmware",
        help="Hysteresis source for plots/inference: firmware CFG or Python derived from calibration stats",
    )
    parser.add_argument(
        "--python-hysteresis-source",
        choices=["python", "raw"],
        default="python",
        help="Series used over CAL samples to derive Python hysteresis stats",
    )
    parser.add_argument(
        "--python-hysteresis-k-sigma",
        type=float,
        default=1.0,
        help="Sigma multiplier for Python hysteresis from CAL stats: in=mean+k*sigma, out=mean-k*sigma",
    )
    parser.add_argument(
        "--python-calibration-mode",
        choices=["legacy", "two-stage"],
        default="legacy",
        help="How Python Kalman params are prepared from CAL file: legacy uses CLI params, two-stage derives R/x0 from CAL",
    )
    parser.add_argument(
        "--python-two-stage-split-ratio",
        type=float,
        default=0.5,
        help="Split ratio for two-stage CAL (A raw-noise, B filtered-hysteresis). Example 0.5 => 50%%/50%%",
    )
    parser.add_argument(
        "--python-two-stage-k-sigma",
        type=float,
        default=1.0,
        help="Sigma multiplier for two-stage Python hysteresis on filtered CAL stage-B",
    )
    parser.add_argument(
        "--hysteresis-width",
        type=float,
        default=None,
        help="If set, override hysteresis with symmetric window around media_calib: in=mean+width, out=mean-width",
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
    parser.add_argument(
        "--distance-ukf",
        choices=["off", "on"],
        default="off",
        help="Enable UKF only for distance curve (state=distance, measurement=RSSI)",
    )
    parser.add_argument(
        "--distance-ukf-source",
        choices=["python", "raw", "esp"],
        default="python",
        help="RSSI source used as measurement for distance UKF",
    )
    parser.add_argument("--distance-ukf-q", type=float, default=0.05, help="UKF process noise Q on distance state")
    parser.add_argument("--distance-ukf-r", type=float, default=9.0, help="UKF measurement noise R on RSSI")
    parser.add_argument("--distance-ukf-alpha", type=float, default=0.1, help="UKF alpha (sigma-point spread)")
    parser.add_argument("--distance-ukf-beta", type=float, default=2.0, help="UKF beta (distribution prior)")
    parser.add_argument("--distance-ukf-kappa", type=float, default=0.0, help="UKF kappa (secondary scaling)")
    parser.add_argument("--distance-ukf-init-m", type=float, default=1.0, help="UKF initial distance state")
    parser.add_argument("--distance-ukf-init-p", type=float, default=1.0, help="UKF initial distance covariance")
    parser.add_argument(
        "--gamma-comparison",
        choices=["off", "on"],
        default="off",
        help="Enable/disable additional relative gamma comparison plot",
    )
    parser.add_argument(
        "--gamma-source",
        choices=["python", "raw", "esp"],
        default="python",
        help="Primary source label used in gamma comparison plot",
    )
    parser.add_argument(
        "--gamma-ukf",
        choices=["off", "on"],
        default="off",
        help="Enable UKF on gamma state (x=gamma, z=RSSI)",
    )
    parser.add_argument(
        "--gamma-ukf-source",
        choices=["python", "raw", "esp"],
        default="python",
        help="RSSI source used as measurement for gamma UKF",
    )
    parser.add_argument("--gamma-ukf-q", type=float, default=0.01, help="Gamma UKF process noise Q")
    parser.add_argument("--gamma-ukf-r", type=float, default=9.0, help="Gamma UKF measurement noise R on RSSI")
    parser.add_argument("--gamma-ukf-alpha", type=float, default=0.1, help="Gamma UKF alpha")
    parser.add_argument("--gamma-ukf-beta", type=float, default=2.0, help="Gamma UKF beta")
    parser.add_argument("--gamma-ukf-kappa", type=float, default=0.0, help="Gamma UKF kappa")
    parser.add_argument("--gamma-ukf-init", type=float, default=0.0, help="Gamma UKF initial state")
    parser.add_argument("--gamma-ukf-init-p", type=float, default=1.0, help="Gamma UKF initial covariance")
    parser.add_argument(
        "--plot-mode",
        choices=["full", "fw-only"],
        default="full",
        help="Plot content mode: full includes Python overlays, fw-only hides all Python lines/thresholds/change markers",
    )
    parser.add_argument(
        "--firmware-events",
        choices=["off", "on"],
        default="off",
        help="Show/hide vertical EVT firmware markers in plots",
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

        if args.linear_kf_print_config:
            print(
                "[KF-LINEAR] "
                f"Q={args.kalman_q} "
                f"R={args.kalman_r} "
                f"x0={args.kalman_x0} "
                f"P0={args.kalman_p0}"
            )

        is_single_file = len(csv_inputs) == 1 and csv_inputs[0].is_file() and input_path.is_file()
        if (args.plot_raw or args.plot_kalman or args.processed_csv) and not is_single_file:
            print("[WARN] Custom output paths are only used in single-file mode. Using defaults for batch mode.")

        total_samples = 0
        ok_files = 0
        failed_files = 0

        for csv_file in csv_inputs:
            try:
                default_raw, default_kalman, default_processed, default_distance, default_gamma = default_outputs_for_input(csv_file)
                out_raw = Path(args.plot_raw) if (args.plot_raw and is_single_file) else default_raw
                out_kalman = Path(args.plot_kalman) if (args.plot_kalman and is_single_file) else default_kalman
                out_processed = Path(args.processed_csv) if (args.processed_csv and is_single_file) else default_processed
                out_distance = default_distance
                out_gamma = default_gamma

                n = process_one_csv(
                    input_csv=csv_file,
                    out_raw=out_raw,
                    out_kalman=out_kalman,
                    out_processed=out_processed,
                    out_distance=out_distance,
                    out_gamma=out_gamma,
                    q=args.kalman_q,
                    r=args.kalman_r,
                    x0=args.kalman_x0,
                    p0=args.kalman_p0,
                    distance_conversion=args.distance_conversion,
                    distance_source=args.distance_source,
                    calibration_distance_m=args.calibration_distance_m,
                    path_loss_exp=args.path_loss_exp,
                    distance_ukf=args.distance_ukf,
                    distance_ukf_source=args.distance_ukf_source,
                    distance_ukf_q=args.distance_ukf_q,
                    distance_ukf_r=args.distance_ukf_r,
                    distance_ukf_alpha=args.distance_ukf_alpha,
                    distance_ukf_beta=args.distance_ukf_beta,
                    distance_ukf_kappa=args.distance_ukf_kappa,
                    distance_ukf_init_m=args.distance_ukf_init_m,
                    distance_ukf_init_p=args.distance_ukf_init_p,
                    hysteresis_source=args.hysteresis_source,
                    py_hysteresis_source=args.python_hysteresis_source,
                    py_hysteresis_k_sigma=args.python_hysteresis_k_sigma,
                    python_calibration_mode=args.python_calibration_mode,
                    python_two_stage_split_ratio=args.python_two_stage_split_ratio,
                    python_two_stage_k_sigma=args.python_two_stage_k_sigma,
                    hysteresis_width=args.hysteresis_width,
                    gamma_comparison=args.gamma_comparison,
                    gamma_source=args.gamma_source,
                    gamma_ukf=args.gamma_ukf,
                    gamma_ukf_source=args.gamma_ukf_source,
                    gamma_ukf_q=args.gamma_ukf_q,
                    gamma_ukf_r=args.gamma_ukf_r,
                    gamma_ukf_alpha=args.gamma_ukf_alpha,
                    gamma_ukf_beta=args.gamma_ukf_beta,
                    gamma_ukf_kappa=args.gamma_ukf_kappa,
                    gamma_ukf_init=args.gamma_ukf_init,
                    gamma_ukf_init_p=args.gamma_ukf_init_p,
                    plot_mode=args.plot_mode,
                    show_firmware_events=(args.firmware_events == "on"),
                )
                total_samples += n
                ok_files += 1
                print(f"[OK] {csv_file} -> samples={n}")
                print(f"     raw={out_raw}")
                print(f"     kalman={out_kalman}")
                print(f"     processed={out_processed}")
                if args.distance_conversion == "on":
                    print(f"     distance={out_distance}")
                if args.distance_ukf == "on":
                    print("     distance_ukf=enabled")
                if args.gamma_comparison == "on":
                    print(f"     gamma={out_gamma}")
                if args.gamma_ukf == "on":
                    print("     gamma_ukf=enabled")
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
