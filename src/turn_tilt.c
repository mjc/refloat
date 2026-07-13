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

#include "turn_tilt.h"

#include "lib/utils.h"

#include <math.h>

void turn_tilt_init(TurnTilt *tt) {
    tt->boost_per_erpm = 0.0f;
    ema_init(&tt->yaw_change);
    smooth_setpoint_init(&tt->setpoint);

    turn_tilt_reset(tt);
}

void turn_tilt_reset(TurnTilt *tt) {
    tt->last_yaw_angle = NAN;
    ema_reset(&tt->yaw_change, 0.0f);
    tt->yaw_aggregate = 0.0f;

    tt->target = 0.0f;
    smooth_setpoint_reset(&tt->setpoint);
}

void turn_tilt_configure(TurnTilt *tt, const RefloatConfig *config, float frequency) {
    ema_configure(&tt->yaw_change, 25.0f, frequency);

    // vTx 3 accepts 65535, while the supported turn-tilt boost is 0..10000%.
    const uint16_t erpm_boost = min(config->turntilt_erpm_boost, 10000u);
    tt->boost_per_erpm = (float) erpm_boost / 100.0 / fmaxf(config->turntilt_erpm_boost_end, 1.0f);

    float speed_time_constant = config->turn_tilt.filter.time_constant * 0.5f;
    smooth_setpoint_configure(
        &tt->setpoint,
        config->turn_tilt.filter.time_constant,
        speed_time_constant,
        speed_time_constant,
        0.2f,
        20.0f,
        20.0f,
        20.0f,
        20.0f,
        frequency
    );
}

void turn_tilt_aggregate(TurnTilt *tt, const IMU *imu, float dt) {
    if (!isfinite(imu->yaw) || dt <= 0.0f) {
        return;
    }

    if (!isfinite(tt->last_yaw_angle)) {
        tt->last_yaw_angle = imu->yaw;
        return;
    }

    float new_change = imu->yaw - tt->last_yaw_angle;
    if (new_change < -180.0f) {
        new_change += 360.0f;
    } else if (new_change > 180.0f) {
        new_change -= 360.0f;
    }

    tt->last_yaw_angle = imu->yaw;

    // limit change to avoid overreactions at low speed
    ema_update(&tt->yaw_change, clampf(new_change / dt, -72.0f, 72.0f));

    // clear the aggregate yaw whenever we change direction
    if (sign(tt->yaw_change.value) != sign(tt->yaw_aggregate)) {
        tt->yaw_aggregate = 0;
    }

    // don't count tiny yaw changes towards aggregate
    if (fabsf(tt->yaw_change.value) > 30.0f) {
        tt->yaw_aggregate += new_change;
    }
}

void turn_tilt_update(
    TurnTilt *tt, const MotorData *md, const RefloatConfig *config, bool wheelslip, float dt
) {
    if (wheelslip) {
        smooth_setpoint_winddown(&tt->setpoint);
        return;
    }

    // Custom config packets bypass the VESC Tool UI's field bounds.
    float angle_limit = clampf(config->turntilt_angle_limit, 0.0f, 30.0f);
    float strength = clampf(config->turntilt_strength, -30.0f, 30.0f);
    float start_angle = clampf(config->turntilt_start_angle, 0.0f, 45.0f);
    // vTx 3 accepts zero even though the editor's minimum is 100 ERPM.
    uint16_t start_erpm = max(config->turntilt_start_erpm, 100u);
    // vTx 1 can encode values below the editor's 50-degree aggregate minimum.
    float yaw_aggregate = clampf(config->turntilt_yaw_aggregate, 50.0f, 255.0f);
    float abs_yaw_change = fabsf(tt->yaw_change.value);
    float abs_yaw_aggregate = fabsf(tt->yaw_aggregate);

    // Minimum threshold based on
    // a) minimum degrees per second (yaw/turn increment)
    // b) minimum yaw aggregate (to filter out wiggling on uneven road)
    if (abs_yaw_aggregate < start_angle || abs_yaw_change < 30.0f) {
        tt->target = 0;
    } else {
        // Calculate desired angle
        tt->target = abs_yaw_change * LOOP_HERTZ_COMPAT_RECIP * strength;

        // Apply speed scaling
        float boost;
        if (md->abs_erpm < config->turntilt_erpm_boost_end) {
            boost = 1.0 + md->abs_erpm * tt->boost_per_erpm;
        } else {
            // Keep the post-ramp path under the same BTLE-reachable bound.
            boost = 1.0 + (float) min(config->turntilt_erpm_boost, 10000u) / 100.0;
        }
        tt->target *= boost;

        // Increase turntilt based on aggregate yaw change (at most: double it)
        float aggregate_damper = 1.0;
        if (md->abs_erpm < 2000) {
            aggregate_damper = 0.5;
        }
        boost = 1 + aggregate_damper * abs_yaw_aggregate / yaw_aggregate;
        boost = fminf(boost, 2);
        tt->target *= boost;

        tt->target = clampf(tt->target, -angle_limit, angle_limit);

        // Disable below erpm threshold otherwise add directionality
        if (md->abs_erpm < start_erpm) {
            tt->target = 0;
        } else {
            tt->target *= md->erpm_sign;
        }
    }

    smooth_setpoint_update(&tt->setpoint, tt->target, md->forward, 1.0f, dt);
}
