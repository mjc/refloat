// Copyright 2025 Lukas Hrazky
//
// This file is part of the Refloat VESC package.
//
// Refloat VESC package is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// Refloat VESC package is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.

#include "pid.h"

#include "lib/utils.h"

void pid_init(PID *pid) {
    ema_init(&pid->p_fwd_scale);
    ema_init(&pid->rate_p_fwd_scale);
    ema_init(&pid->p_bwd_scale);
    ema_init(&pid->rate_p_bwd_scale);

    pid_reset(pid);
}

void pid_reset(PID *pid) {
    pid->p = 0;
    pid->i = 0;
    pid->rate_p = 0;

    pid->p_fwd_scale.value = 1.0f;
    pid->rate_p_fwd_scale.value = 1.0f;
    pid->p_bwd_scale.value = 1.0f;
    pid->rate_p_bwd_scale.value = 1.0f;
}

void pid_configure(PID *pid, float frequency) {
    ema_configure(&pid->p_fwd_scale, 1.0f, frequency);
    ema_configure(&pid->rate_p_fwd_scale, 1.0f, frequency);
    ema_configure(&pid->p_bwd_scale, 1.0f, frequency);
    ema_configure(&pid->rate_p_bwd_scale, 1.0f, frequency);
}

void pid_update(
    PID *pid,
    float setpoint,
    const MotorData *md,
    const IMU *imu,
    const RefloatConfig *config,
    float dt
) {
    if (dt <= 0.0f) {
        pid->p = 0.0f;
        pid->rate_p = 0.0f;
        return;
    }

    float error = setpoint - imu->balance_pitch;

    // XML imports and COMM_SET_CUSTOM_CONFIG over BTLE bypass VESC Tool's
    // editor bounds. Keep P, I, rate-P, I windup, and both brake scales in
    // their declared control domains before they affect requested current.
    pid->p = error * clampf(config->kp, 0.0f, 40.0f) * TORQUE_CONSTANT_COMPAT;

    pid->i = pid->i +
        error * clampf(config->ki, 0.0f, 0.5f) * TORQUE_CONSTANT_COMPAT * LOOP_HERTZ_COMPAT * dt;
    float ki_limit = clampf(config->ki_limit, 0.0f, 500.0f) * TORQUE_CONSTANT_COMPAT;
    if (ki_limit > 0 && fabsf(pid->i) > ki_limit) {
        pid->i = ki_limit * sign(pid->i);
    }

    pid->rate_p = -imu->pitch_rate * clampf(config->kp2, 0.0f, 3.0f) * TORQUE_CONSTANT_COMPAT;

    // brake scale coefficient smoothing
    if (md->erpm < -500) {
        ema_update(&pid->p_fwd_scale, clampf(config->kp_brake, 0.0f, 3.0f));
        ema_update(&pid->rate_p_fwd_scale, clampf(config->kp2_brake, 0.0f, 3.0f));
    } else {
        ema_update(&pid->p_fwd_scale, 1.0f);
        ema_update(&pid->rate_p_fwd_scale, 1.0f);
    }

    if (md->erpm > 500) {
        ema_update(&pid->p_bwd_scale, clampf(config->kp_brake, 0.0f, 3.0f));
        ema_update(&pid->rate_p_bwd_scale, clampf(config->kp2_brake, 0.0f, 3.0f));
    } else {
        ema_update(&pid->p_bwd_scale, 1.0f);
        ema_update(&pid->rate_p_bwd_scale, 1.0f);
    }

    pid->p *= pid->p > 0 ? pid->p_fwd_scale.value : pid->p_bwd_scale.value;
    pid->rate_p *= pid->rate_p > 0 ? pid->rate_p_fwd_scale.value : pid->rate_p_bwd_scale.value;
}
