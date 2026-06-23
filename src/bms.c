// Copyright 2024 Syler Clayton
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

#include "bms.h"

#include "lib/utils.h"

#include <math.h>

void bms_init(BMS *bms) {
    bms->cell_lv = 0.0f;
    bms->cell_hv = 0.0f;
    bms->cell_lt = 0;
    bms->cell_ht = 0;
    bms->bms_ht = 0;
    bms->msg_age = 42.0f;
    bms->fault_mask = BMSF_NONE;
}

static inline void set_fault(uint32_t *fault_mask, BMSFaultCode fault_code) {
    *fault_mask |= 1u << (fault_code - 1);
}

void bms_update(BMS *bms, const CfgBMS *cfg, const Time *time) {
    if (!cfg->enabled) {
        bms->fault_mask = BMSF_NONE;
        return;
    }

    uint32_t fault_mask = BMSF_NONE;
    const float timeout = 5.0f;
    // Custom config packets bypass the VESC Tool UI's field bounds.
    const float cell_lv_threshold = fmaxf(cfg->cell_lv_threshold, 2.5f);
    const float cell_hv_threshold = clampf(cfg->cell_hv_threshold, 2.5f, 4.5f);
    const float cell_balance_threshold = clampf(cfg->cell_balance_threshold, 0.01f, 1.0f);
    const int16_t cell_ht_threshold = cfg->cell_ht_threshold > 60 ? 60 : cfg->cell_ht_threshold;
    const int16_t cell_lt_threshold = (int16_t) clampf(cfg->cell_lt_threshold, -20.0f, 20.0f);
    const int16_t bms_ht_threshold = cfg->bms_ht_threshold > 80 ? 80 : cfg->bms_ht_threshold;

    // Before the first BMS update occurs right after startup, msg_age has its
    // init value. We need to wait the `timeout` time before issuing errors.
    if (bms->msg_age > timeout) {
        if (time_elapsed(time, start, timeout)) {
            set_fault(&fault_mask, BMSF_CONNECTION);
        }
        bms->fault_mask = fault_mask;
        return;
    }

    if (bms->cell_lv < cell_lv_threshold) {
        set_fault(&fault_mask, BMSF_CELL_UNDER_VOLTAGE);
    }

    if (bms->cell_hv > cell_hv_threshold) {
        set_fault(&fault_mask, BMSF_CELL_OVER_VOLTAGE);
    }

    // Setting high temp threshold to 0 disables both high and low temp checking
    if (cell_ht_threshold > 0) {
        if (bms->cell_ht > cell_ht_threshold) {
            set_fault(&fault_mask, BMSF_CELL_OVER_TEMP);
        }

        if (bms->cell_lt < cell_lt_threshold) {
            set_fault(&fault_mask, BMSF_CELL_UNDER_TEMP);
        }
    }

    if (bms_ht_threshold > 0 && bms->bms_ht > bms_ht_threshold) {
        set_fault(&fault_mask, BMSF_OVER_TEMP);
    }

    if (fabsf(bms->cell_lv - bms->cell_hv) > cell_balance_threshold) {
        set_fault(&fault_mask, BMSF_CELL_BALANCE);
    }

    bms->fault_mask = fault_mask;
}

bool bms_is_fault(const BMS *bms, BMSFaultCode fault_code) {
    if (fault_code < BMSF_CONNECTION || fault_code > BMSF_CELL_BALANCE) {
        return false;
    }
    return (bms->fault_mask & (1u << (fault_code - 1))) != 0;
}
