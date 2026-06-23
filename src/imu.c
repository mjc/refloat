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

#include "imu.h"

#include "lib/utils.h"

#include "vesc_c_if.h"

void imu_init(IMU *imu) {
    imu->pitch = 0.0f;
    imu->balance_pitch = 0.0f;
    imu->roll = 0.0f;
    imu->yaw = 0.0f;
    imu->pitch_rate = 0.0f;

    imu->flywheel_pitch_offset = 0.0f;
    imu->flywheel_roll_offset = 0.0f;
}

void imu_update(IMU *imu, const BalanceFilterData *bf, const State *state) {
    float roll_rad = VESC_IF->imu_get_roll();  // in Radians
    if (!isfinite(roll_rad)) {
        roll_rad = 0.0f;
    }

    float pitch_rad = VESC_IF->imu_get_pitch();
    float yaw_rad = VESC_IF->imu_get_yaw();
    if (!isfinite(pitch_rad)) {
        pitch_rad = 0.0f;
    }
    if (!isfinite(yaw_rad)) {
        yaw_rad = 0.0f;
    }
    imu->pitch = rad2deg(pitch_rad);
    imu->roll = rad2deg(roll_rad);
    imu->yaw = rad2deg(yaw_rad);
    float balance_pitch = balance_filter_get_pitch(bf);
    imu->balance_pitch = isfinite(balance_pitch) ? rad2deg(balance_pitch) : 0.0f;

    float gyro[3];
    VESC_IF->imu_get_gyro(gyro);
    for (size_t i = 0; i < 3; ++i) {
        if (!isfinite(gyro[i])) {
            gyro[i] = 0.0f;
        }
    }

    float sin_roll = sinf(roll_rad);
    float cos_roll = cosf(roll_rad);

    // Rotated to diminish influence of Yaw Change on Gyro Y when board is rolled
    // (Estimates Pitch Rate solely due to rider input, without influence from board turning)
    imu->pitch_rate = cos_roll * cos_roll * gyro[1] + sin_roll * cos_roll * gyro[2];
    if (state->darkride) {
        imu->pitch_rate = -imu->pitch_rate;
    }

    if (state->mode == MODE_FLYWHEEL) {
        imu->pitch = imu->flywheel_pitch_offset - imu->pitch;
        imu->balance_pitch = imu->pitch;
        imu->roll -= imu->flywheel_roll_offset;
        if (imu->roll < -200) {
            imu->roll += 360;
        } else if (imu->roll > 200) {
            imu->roll -= 360;
        }
    }
}

void imu_set_flywheel_offsets(IMU *imu) {
    imu->flywheel_pitch_offset = imu->pitch;
    imu->flywheel_roll_offset = imu->roll;
}
