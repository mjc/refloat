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

#include "motor_data.h"

#include "lib/utils.h"

#include "vesc_c_if.h"

#include <math.h>

void motor_data_init(MotorData *m) {
    m->erpm = 0.0f;
    m->abs_erpm = 0.0f;
    m->last_erpm = 0.0f;
    m->erpm_sign = 1;
    ema_init(&m->abs_erpm_smooth);

    m->speed = 0.0f;
    m->distance = 0.0f;
    m->erpm_to_speed = 0.0f;

    m->current = 0.0f;
    m->dir_current = 0.0f;
    biquad_init(&m->filt_current);
    m->torque = 0.0f;
    m->braking = false;
    m->forward = true;

    m->duty_raw = 0.0f;
    ema_init(&m->duty_cycle);
    sma_init(&m->acceleration);

    ema_init(&m->batt_current);
    m->batt_voltage = 0.0f;
    m->motor_current_saturation = 0.0f;
    m->battery_current_saturation = 0.0f;

    m->mosfet_temp = 0.0f;
    m->motor_temp = 0.0f;

    m->current_min = 0.0f;
    m->current_max = 0.0f;
    m->battery_current_min = 0.0f;
    m->battery_current_max = 0.0f;
    m->mosfet_temp_max = 0.0f;
    m->motor_temp_max = 0.0f;
    m->duty_max_with_margin = 0.0f;
    m->lv_threshold = 0.0f;
    m->hv_threshold = 0.0f;
    m->speed_constant = 0.0f;

    motor_data_reset(m);
}

void motor_data_destroy(MotorData *m) {
    sma_destroy(&m->acceleration);
}

void motor_data_reset(MotorData *m) {
    ema_reset(&m->duty_cycle, 0.0f);
    sma_reset(&m->acceleration);
    biquad_reset(&m->filt_current);
}

void motor_data_refresh_motor_config(MotorData *m, float lv_threshold, float hv_threshold) {
    uint8_t battery_cells = VESC_IF->get_cfg_int(CFG_PARAM_si_battery_cells);
    if (battery_cells > 0) {
        if (lv_threshold < 10) {
            lv_threshold *= battery_cells;
        }
        if (hv_threshold < 10) {
            hv_threshold *= battery_cells;
        }
    }

    m->lv_threshold = lv_threshold;
    m->hv_threshold = hv_threshold;

    // This guards the C/Lisp API boundary, not VESC Tool config input. VESC's
    // Lisp C API can write malformed values directly; keep them out of current
    // and safety-limit calculations.
    float current_min = VESC_IF->get_cfg_float(CFG_PARAM_l_current_min);
    float current_max = VESC_IF->get_cfg_float(CFG_PARAM_l_current_max);
    float battery_current_min = VESC_IF->get_cfg_float(CFG_PARAM_l_in_current_min);
    float battery_current_max = VESC_IF->get_cfg_float(CFG_PARAM_l_in_current_max);
    float mosfet_temp_start = VESC_IF->get_cfg_float(CFG_PARAM_l_temp_fet_start);
    float motor_temp_start = VESC_IF->get_cfg_float(CFG_PARAM_l_temp_motor_start);
    float max_duty = VESC_IF->get_cfg_float(CFG_PARAM_l_max_duty);

    // min motor current is a positive value here!
    m->current_min = isfinite(current_min) ? fabsf(current_min) : 0.0f;
    m->current_max = isfinite(current_max) && current_max > 0.0f ? current_max : 0.0f;
    m->battery_current_min = isfinite(battery_current_min) ? fabsf(battery_current_min) : 0.0f;
    m->battery_current_max =
        isfinite(battery_current_max) && battery_current_max > 0.0f ? battery_current_max : 0.0f;
    m->mosfet_temp_max = isfinite(mosfet_temp_start) ? mosfet_temp_start - 3.0f : 0.0f;
    m->motor_temp_max = isfinite(motor_temp_start) ? motor_temp_start - 3.0f : 0.0f;
    m->duty_max_with_margin = isfinite(max_duty) ? max_duty - 0.05f : 0.0f;

    // On firmware < 6.06 flux linkage is not exposed on the interface, 0 is returned
    float flux_linkage = VESC_IF->get_cfg_float(CFG_PARAM_foc_motor_flux_linkage);
    int motor_poles = VESC_IF->get_cfg_int(CFG_PARAM_si_motor_poles);
    if (isfinite(flux_linkage) && flux_linkage > 0.001f && motor_poles > 0) {
        m->speed_constant = 1 / (1.5f * 0.5 * motor_poles * flux_linkage);
    } else {
        m->speed_constant = 1 / TORQUE_CONSTANT_COMPAT;
    }

    float gear_ratio = VESC_IF->get_cfg_float(CFG_PARAM_si_gear_ratio);
    float wheel_diameter = VESC_IF->get_cfg_float(CFG_PARAM_si_wheel_diameter);
    if (motor_poles > 0 && isfinite(gear_ratio) && gear_ratio > 0.0f && isfinite(wheel_diameter) &&
        wheel_diameter > 0.0f) {
        float mechanical_rpm_per_erpm = 1.0f / (0.5f * motor_poles * gear_ratio);
        m->erpm_to_speed =
            mechanical_rpm_per_erpm * (float) M_PI * wheel_diameter * 60.0f / 1000.0f;
    } else {
        m->erpm_to_speed = 0.0f;
    }
}

void motor_data_configure(MotorData *m, float current_cutoff_freq, float frequency) {
    ema_configure(&m->abs_erpm_smooth, 10.0f, frequency);

    // Zero historically selected the 20Hz default; raw custom configs can
    // also exceed the filter's declared 1..20Hz domain.
    current_cutoff_freq = current_cutoff_freq < 1.0f ? 20.0f : fminf(current_cutoff_freq, 20.0f);
    biquad_configure(&m->filt_current, BQ_LOWPASS, current_cutoff_freq, frequency);

    ema_configure(&m->duty_cycle, 1.0f, frequency);
    sma_configure(&m->acceleration, 8.0f, frequency);
    ema_configure(&m->batt_current, 1.0f, frequency);
}

void motor_data_update(MotorData *m, float dt) {
    float erpm = VESC_IF->mc_get_rpm();
    float speed =
        m->erpm_to_speed != 0.0f ? erpm * m->erpm_to_speed : VESC_IF->mc_get_speed() * 3.6f;
    float distance = VESC_IF->mc_get_distance();
    float current = VESC_IF->mc_get_tot_current_filtered();
    float dir_current = VESC_IF->mc_get_tot_current_directional_filtered();
    float duty = VESC_IF->mc_get_duty_cycle_now();
    float batt_current = VESC_IF->mc_get_tot_current_in_filtered();
    float voltage = VESC_IF->mc_get_input_voltage_filtered();
    float mosfet_temp = VESC_IF->mc_temp_fet_filtered();
    float motor_temp = VESC_IF->mc_temp_motor_filtered();
    if (isfinite(erpm)) {
        m->erpm = erpm;
        m->abs_erpm = fabsf(erpm);
        m->erpm_sign = sign(erpm);
        ema_update(&m->abs_erpm_smooth, m->abs_erpm);
    }

    if (!isfinite(speed)) {
        speed = m->speed;
    } else {
        m->speed = speed;
    }
    if (isfinite(distance)) {
        m->distance = distance;
    }
    if (isfinite(current)) {
        m->current = current;
        m->braking = current < 0;
    }
    if (isfinite(dir_current)) {
        m->dir_current = dir_current;
        biquad_update(&m->filt_current, m->dir_current);
        if (m->speed_constant != 0.0f) {
            m->torque = m->filt_current.value / m->speed_constant;
        }
    }
    if (isfinite(duty)) {
        m->duty_raw = fabsf(duty);
        ema_update(&m->duty_cycle, m->duty_raw);
    }

    if (isfinite(erpm) && isfinite(dt) && dt > 0.0f) {
        sma_update(&m->acceleration, (m->erpm - m->last_erpm) / dt);
    }
    if (isfinite(erpm)) {
        m->last_erpm = m->erpm;
        if (m->abs_erpm > 250 || m->torque < 18.0f) {
            m->forward = m->erpm >= 0.0f;
        } else {
            m->forward = m->torque >= 0.0f;
        }
    }

    if (isfinite(batt_current)) {
        ema_update(&m->batt_current, batt_current);
    }
    if (isfinite(voltage)) {
        m->batt_voltage = voltage;
    }

    float motor_current_limit = m->braking ? m->current_min : m->current_max;
    if (motor_current_limit > 0.0f) {
        m->motor_current_saturation = fabsf(m->filt_current.value) / motor_current_limit;
    } else {
        m->motor_current_saturation = 0.0f;
    }

    float battery_current_limit =
        m->batt_current.value < 0 ? m->battery_current_min : m->battery_current_max;
    if (battery_current_limit > 0.0f) {
        m->battery_current_saturation = fabsf(m->batt_current.value) / battery_current_limit;
    } else {
        m->battery_current_saturation = 0.0f;
    }

    if (isfinite(mosfet_temp)) {
        m->mosfet_temp = mosfet_temp;
    }
    if (isfinite(motor_temp)) {
        m->motor_temp = motor_temp;
    }
}

void motor_data_evaluate_alerts(const MotorData *m, AlertTracker *at, const Time *time) {
    unused(m);

    mc_fault_code fault_code = VESC_IF->mc_get_fault();
    if (fault_code != FAULT_CODE_NONE) {
        alert_tracker_add(at, time, ALERT_FW_FAULT, fault_code);
    }
}

float motor_data_get_current_saturation(const MotorData *m) {
    return max(m->motor_current_saturation, m->battery_current_saturation);
}

float motor_data_torque_to_current(const MotorData *m, float torque) {
    return torque * m->speed_constant;
}
