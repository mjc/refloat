// Copyright 2024 Lukas Hrazky
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

#include "booster.h"

#include "lib/utils.h"

#include <math.h>

void booster_init(Booster *b) {
    ema_init(&b->torque);
    booster_reset(b);
}

void booster_reset(Booster *b) {
    ema_reset(&b->torque, 0.0f);
}

void booster_configure(Booster *b, float frequency) {
    ema_configure(&b->torque, 1.0f, frequency);
}

void booster_update(
    Booster *b, const MotorData *md, const RefloatConfig *config, float proportional
) {
    float torque;
    float angle;
    float ramp;
    // Custom configs can bypass the current bounds. The legacy BTLE tuning
    // protocol permits a 17-degree ramp, wider than the current XML editor.
    // Booster angle is intentionally not upper-bounded: handtest/flywheel use
    // 100 degrees as the established disable sentinel.
    if (md->braking) {
        torque = clampf(config->brkbooster_current, 0.0f, 100.0f) * TORQUE_CONSTANT_COMPAT;
        angle = fmaxf(config->brkbooster_angle, 0.0f);
        ramp = clampf(config->brkbooster_ramp, 1.0f, 17.0f);
    } else {
        torque = clampf(config->booster_current, 0.0f, 100.0f) * TORQUE_CONSTANT_COMPAT;
        angle = fmaxf(config->booster_angle, 0.0f);
        ramp = clampf(config->booster_ramp, 1.0f, 17.0f);
    }

    // Make booster a bit stronger at higher speed (up to 2x stronger when braking)
    const int boost_min_erpm = 3000;
    if (md->abs_erpm > boost_min_erpm) {
        float speedstiffness = fminf(1, (md->abs_erpm - boost_min_erpm) / 10000);
        if (md->braking) {
            // use higher current at speed when braking
            torque += torque * speedstiffness;
        } else {
            // when accelerating, we reduce the booster start angle as we get faster
            // strength remains unchanged
            float angledivider = 1 + speedstiffness;
            angle /= angledivider;
        }
    }

    float abs_proportional = fabsf(proportional);
    if (abs_proportional > angle) {
        if (abs_proportional - angle < ramp) {
            torque *= sign(proportional) * (abs_proportional - angle) / ramp;
        } else {
            torque *= sign(proportional);
        }
    } else {
        torque = 0;
    }

    ema_update(&b->torque, torque);
}
