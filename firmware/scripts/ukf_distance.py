#!/usr/bin/env python3
"""
Scalar Unscented Kalman Filter (UKF) utilities for distance estimation.

This module is intentionally written as a generic 1D UKF with explicit,
well-commented steps so you can adapt the process/measurement models easily.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable


@dataclass
class UKFStepResult:
    """Stores the UKF posterior estimate for one update step."""

    x_post: float
    p_post: float


class ScalarUnscentedKalmanFilter:
    """
    Scalar UKF (n=1) with configurable sigma-point parameters.

    Parameter mapping:
    - alpha: spread of sigma points around the mean.
    - beta: prior knowledge about distribution (2.0 is common for Gaussian).
    - kappa: secondary scaling parameter.
    - x0: initial state estimate.
    - p0: initial covariance estimate.

    State model and measurement model are provided per-step:
    - f(x): process model (state transition).
    - h(x): measurement model (non-linear observation mapping).

    Noise terms are also provided per-step:
    - q: process noise covariance.
    - r: measurement noise covariance.
    """

    def __init__(
        self,
        *,
        alpha: float = 1e-1,
        beta: float = 2.0,
        kappa: float = 0.0,
        x0: float = 1.0,
        p0: float = 1.0,
    ) -> None:
        if alpha <= 0.0:
            raise ValueError("alpha must be > 0")
        if p0 <= 0.0:
            raise ValueError("p0 must be > 0")

        self.n = 1.0
        self.alpha = float(alpha)
        self.beta = float(beta)
        self.kappa = float(kappa)
        self.lambda_ = self.alpha * self.alpha * (self.n + self.kappa) - self.n

        # Sigma-point weights for mean and covariance.
        denom = self.n + self.lambda_
        if denom <= 0.0:
            raise ValueError("Invalid alpha/kappa combination: n + lambda <= 0")
        self.w_m0 = self.lambda_ / denom
        self.w_c0 = self.w_m0 + (1.0 - self.alpha * self.alpha + self.beta)
        self.w_i = 1.0 / (2.0 * denom)

        self.x = float(x0)
        self.p = float(p0)

    def _sigma_points(self, x_mean: float, p_cov: float) -> tuple[float, float, float]:
        """Builds 3 sigma points for scalar state: x, x+delta, x-delta."""
        p_safe = max(p_cov, 1e-12)
        delta = math.sqrt((self.n + self.lambda_) * p_safe)
        return x_mean, x_mean + delta, x_mean - delta

    def step(
        self,
        *,
        z: float,
        f: Callable[[float], float],
        h: Callable[[float], float],
        q: float,
        r: float,
    ) -> UKFStepResult:
        """
        Runs one complete UKF iteration (predict + update).

        Args:
        - z: measurement at current step.
        - f: process model x_k = f(x_{k-1}).
        - h: measurement model z_k = h(x_k).
        - q: process noise covariance.
        - r: measurement noise covariance.
        """
        q_safe = max(float(q), 1e-12)
        r_safe = max(float(r), 1e-12)

        # 1) Build sigma points from prior (x, p).
        x0, x1, x2 = self._sigma_points(self.x, self.p)

        # 2) Predict sigma points through process model f.
        fx0 = f(x0)
        fx1 = f(x1)
        fx2 = f(x2)

        # 3) Predicted state mean.
        x_pred = self.w_m0 * fx0 + self.w_i * fx1 + self.w_i * fx2

        # 4) Predicted covariance + process noise.
        p_pred = (
            self.w_c0 * (fx0 - x_pred) ** 2
            + self.w_i * (fx1 - x_pred) ** 2
            + self.w_i * (fx2 - x_pred) ** 2
            + q_safe
        )

        # 5) Recompute sigma points around predicted state.
        px0, px1, px2 = self._sigma_points(x_pred, p_pred)

        # 6) Push predicted sigma points through measurement model h.
        hz0 = h(px0)
        hz1 = h(px1)
        hz2 = h(px2)

        # 7) Predicted measurement mean.
        z_pred = self.w_m0 * hz0 + self.w_i * hz1 + self.w_i * hz2

        # 8) Measurement covariance + measurement noise.
        s_cov = (
            self.w_c0 * (hz0 - z_pred) ** 2
            + self.w_i * (hz1 - z_pred) ** 2
            + self.w_i * (hz2 - z_pred) ** 2
            + r_safe
        )

        # 9) Cross covariance between state and measurement.
        p_xz = (
            self.w_c0 * (px0 - x_pred) * (hz0 - z_pred)
            + self.w_i * (px1 - x_pred) * (hz1 - z_pred)
            + self.w_i * (px2 - x_pred) * (hz2 - z_pred)
        )

        # 10) Kalman gain.
        k_gain = p_xz / max(s_cov, 1e-12)

        # 11) Posterior update.
        self.x = x_pred + k_gain * (float(z) - z_pred)
        self.p = max(p_pred - k_gain * s_cov * k_gain, 1e-12)

        return UKFStepResult(x_post=self.x, p_post=self.p)


def distance_to_rssi_log(
    *,
    distance_m: float,
    ref_rssi_at_calib: float,
    calibration_distance_m: float,
    path_loss_exp: float,
) -> float:
    """
    Generic non-linear measurement model h(x) for log-distance path loss.

    Mapping used here:
    - state x := distance_m
    - measurement z := rssi (dBm)

    h(x) = RSSI_ref - 10*n*log10(x / d_ref)

    where:
    - RSSI_ref: measured RSSI at calibration distance d_ref.
    - n: path-loss exponent.
    """
    d = max(float(distance_m), 1e-6)
    d_ref = max(float(calibration_distance_m), 1e-6)
    n = max(float(path_loss_exp), 1e-6)
    return float(ref_rssi_at_calib) - 10.0 * n * math.log10(d / d_ref)
