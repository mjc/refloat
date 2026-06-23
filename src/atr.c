// Copyright 2022 Dado Mista
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

#include "atr.h"

#include "lib/utils.h"

#include <math.h>

void atr_init(ATR *atr) {
    atr->speed_boost_mult = 0.0f;
    atr->ad_alpha1 = 0.0f;
    atr->ad_alpha2 = 0.0f;
    atr->ad_alpha3 = 0.0f;

    ema_init(&atr->transition_target);
    smooth_setpoint_init(&atr->setpoint);

    atr_reset(atr);
}

void atr_reset(ATR *atr) {
    atr->accel_diff = 0.0f;
    atr->speed_boost = 0.0f;

    atr->target = 0.0f;
    ema_reset(&atr->transition_target, 0.0f);
    atr->transition_boost = 1.0f;
    smooth_setpoint_reset(&atr->setpoint);
}

void atr_configure(ATR *atr, const RefloatConfig *config, float frequency) {
    float speed_boost = clampf(config->atr_speed_boost, -1.0f, 1.0f);
    atr->speed_boost_mult = 1.0f / 3000.0f;
    if (fabsf(speed_boost) > 0.4f) {
        // above 0.4 we add 500erpm for each extra 10% of speed boost, so at
        // most +6000 for 100% speed boost
        atr->speed_boost_mult = 1.0f / ((fabsf(speed_boost) - 0.4f) * 5000 + 3000.0f);
    }

    atr->ad_alpha3 = ema_calculate_alpha(10.0f, frequency);
    atr->ad_alpha2 = ema_calculate_alpha(6.0f, frequency);
    atr->ad_alpha1 = ema_calculate_alpha(1.0f, frequency);

    ema_configure(&atr->transition_target, 6.0f, frequency);
    // Programmatic/XML custom config updates can exceed the declared 100
    // degree/s ATR engage and release limits before serialization over BTLE.
    smooth_setpoint_configure(
        &atr->setpoint,
        config->atr.filter.time_constant,
        config->atr.filter.on_speed_time_constant,
        config->atr.filter.off_speed_time_constant,
        0.2f,
        clampf(config->atr.filter.on_speed_limit, 0.0f, 100.0f),
        clampf(config->atr.filter.off_speed_limit, 0.0f, 100.0f),
        clampf(config->atr.filter.on_speed_limit, 0.0f, 100.0f),
        clampf(config->atr.filter.off_speed_limit, 0.0f, 100.0f),
        frequency
    );
}

void atr_update(
    ATR *atr, const MotorData *motor, const RefloatConfig *config, bool wheelslip, float dt
) {
    if (wheelslip) {
        smooth_setpoint_winddown(&atr->setpoint);
        ema_reset(&atr->transition_target, atr->setpoint.value);
        return;
    }

    // Legacy BTLE tuning reaches 7.5 degrees, wider than the XML editor's
    // current 5-degree threshold range.
    float atr_threshold =
        clampf(motor->braking ? config->atr_threshold_down : config->atr_threshold_up, 0.0f, 7.5f);

    float abs_torque = fabsf(motor->torque);
    float torque_offset = 8 * TORQUE_CONSTANT_COMPAT;  // hard-code to 8A
    // Positive ratios below the configured 5A/4A minima exaggerate expected
    // acceleration; custom config can supply them even though the UI cannot.
    float accel_ratio = motor->braking ? clampf(config->atr_amps_decel_ratio, 4.0f, 30.0f)
                                       : clampf(config->atr_amps_accel_ratio, 5.0f, 30.0f);
    float accel_factor = accel_ratio * TORQUE_CONSTANT_COMPAT;
    float accel_factor2 = accel_factor * 1.3;

    // compare measured acceleration to expected acceleration
    float measured_acc = clampf(motor->acceleration.value * LOOP_HERTZ_COMPAT_RECIP, -5.0f, 5.0f);

    // expected acceleration is proportional to current (minus an offset, required to
    // balance/maintain speed)
    float expected_acc;
    if (abs_torque < 15) {
        expected_acc = (motor->torque - motor->erpm_sign * torque_offset) / accel_factor;
    } else {
        // primitive linear approximation of non-linear torque-accel relationship
        int torque_sign = sign(motor->torque);
        expected_acc = (torque_sign * 15 - motor->erpm_sign * torque_offset) / accel_factor;
        expected_acc += torque_sign * (abs_torque - 15) / accel_factor2;
    }

    float new_accel_diff = expected_acc - measured_acc;
    if (motor->abs_erpm > 250) {
        float alpha = atr->ad_alpha3;
        if (motor->abs_erpm > 2000) {
            alpha = atr->ad_alpha1;
        } else if (motor->abs_erpm > 1000) {
            alpha = atr->ad_alpha2;
        }
        atr->accel_diff += alpha * (new_accel_diff - atr->accel_diff);
    } else {
        atr->accel_diff = 0;
    }

    // atr->accel_diff | > 0  | <= 0
    // -------------+------+-------
    //         forward | up   | down
    //        !forward | down | up
    // Both ATR directions are defined for strengths through 3.5.
    float atr_strength = clampf(
        motor->forward == (atr->accel_diff > 0) ? config->atr_strength_up
                                                : config->atr_strength_down,
        0.0f,
        3.5f
    );

    // from 3000 to 6000..9000 erpm gradually crank up the torque response
    if (motor->abs_erpm > 3000 && !motor->braking) {
        float speed_boost_mult = (motor->abs_erpm - 3000.0f) * atr->speed_boost_mult;
        // configured speedboost can now also be negative (-1..1)
        // -1 brings it to 0 (if erpm exceeds 9000)
        // +1 doubles it     (if erpm exceeds 9000)
        atr->speed_boost =
            fminf(1, speed_boost_mult) * clampf(config->atr_speed_boost, -1.0f, 1.0f);
        atr_strength += atr_strength * atr->speed_boost;
    } else {
        atr->speed_boost = 0.0f;
    }

    // now ATR target is purely based on gap between expected and actual acceleration
    float new_atr_target = atr_strength * atr->accel_diff;
    if (fabsf(new_atr_target) < atr_threshold) {
        new_atr_target = 0;
    } else {
        new_atr_target -= sign(new_atr_target) * atr_threshold;
    }

    float angle_limit = clampf(config->atr_angle_limit, 0.0f, 30.0f);
    // new_atr_target is finite because all inputs above are bounded finite telemetry/config values.
    atr->target = clampf(new_atr_target, -angle_limit, angle_limit);  // GCOVR_EXCL_BR_LINE

    ema_update(&atr->transition_target, atr->target);

    float transition_target = atr->transition_target.value;
    float degrees_diff = fabsf(atr->setpoint.value - transition_target) - 1.0f;
    // Only apply transition boost if the setpoint and target differ in
    // signs and the degree diff is greater than 1
    if (atr->setpoint.value * transition_target < 0 && degrees_diff > 0.0f) {
        // Scale the transition multiplier linearly from 1 to 2 degrees of difference
        // Custom configs can exceed the transition multiplier's 1..4 domain.
        atr->transition_boost = 1.0f +
            min(degrees_diff, 1.0f) * (clampf(config->atr.transition_boost, 1.0f, 4.0f) - 1.0f);
    } else {
        atr->transition_boost = 1.0f;
    }

    smooth_setpoint_update(&atr->setpoint, atr->target, motor->forward, atr->transition_boost, dt);
}
