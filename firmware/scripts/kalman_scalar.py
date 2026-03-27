#!/usr/bin/env python3
"""
Minimal 1D Kalman filter (scalar) for educational inspection.

State model (random walk):
    x_k = x_{k-1} + w,    w ~ N(0, Q)
Measurement model:
    z_k = x_k + v,        v ~ N(0, R)
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class KalmanStep:
    index: int
    z: float
    x_prior: float
    p_prior: float
    innovation: float
    innovation_var: float
    k_gain: float
    x_post: float
    p_post: float


class ScalarKalmanFilter:
    """Simple scalar Kalman filter with explicit step outputs."""

    def __init__(self, q: float, r: float, x0: float, p0: float) -> None:
        if q <= 0:
            raise ValueError("q must be > 0")
        if r <= 0:
            raise ValueError("r must be > 0")
        if p0 <= 0:
            raise ValueError("p0 must be > 0")

        self.q = float(q)
        self.r = float(r)
        self.x = float(x0)
        self.p = float(p0)
        self.index = 0

    def step(self, measurement: float) -> KalmanStep:
        z = float(measurement)

        # Predict
        x_prior = self.x
        p_prior = self.p + self.q

        # Update
        innovation = z - x_prior
        innovation_var = p_prior + self.r
        k_gain = p_prior / innovation_var
        x_post = x_prior + k_gain * innovation
        p_post = (1.0 - k_gain) * p_prior

        self.x = x_post
        self.p = p_post

        step = KalmanStep(
            index=self.index,
            z=z,
            x_prior=x_prior,
            p_prior=p_prior,
            innovation=innovation,
            innovation_var=innovation_var,
            k_gain=k_gain,
            x_post=x_post,
            p_post=p_post,
        )
        self.index += 1
        return step
