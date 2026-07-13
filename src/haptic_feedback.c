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

#include "haptic_feedback.h"

#include "conf/datatypes.h"
#include "lib/utils.h"
#include "vesc_c_if.h"

#include <math.h>

#define TONE_LENGTH 0.1f
#define AUDIBLE_FREQUENCY_MIN 300.0f
#define AUDIBLE_FREQUENCY_MAX 1000.0f
#define AUDIBLE_STRENGTH_MAX 12.0f
#define VIBRATE_FREQUENCY_MIN 10
#define VIBRATE_FREQUENCY_MAX 200
#define VIBRATE_STRENGTH_MAX 25.0f

void haptic_feedback_init(HapticFeedback *hf) {
    hf->type_playing = HAPTIC_FEEDBACK_NONE;
    hf->tone_timer = 0;
    hf->is_playing = false;
    hf->can_change_type = true;
}

void haptic_feedback_configure(HapticFeedback *hf, const RefloatConfig *cfg) {
    hf->cfg = &cfg->haptic;
    // Signed float16 custom configs can bypass both editors' zero minima.
    hf->duty_solid_threshold =
        clampf(cfg->tiltback_duty, 0.0f, 1.0f) + clampf(hf->cfg->duty_solid_offset, 0.0f, 1.0f);

    // pre-calculate the coefficients of the polynomial given the configured max strength speed
    // vTx 1 can carry 0..255, wider than the editor's 10..100 km/h range.
    float m = clampf(hf->cfg->max_strength_speed, 10.0f, 100.0f);
    hf->min_strength = clampf(hf->cfg->min_strength, 0.0f, 1.0f);
    float curvature = clampf(hf->cfg->strength_curvature, 0.0f, 1.0f);
    hf->str_poly_b = (1 - curvature) * (1 - hf->min_strength) / m;
    hf->str_poly_c = (1 - hf->min_strength - hf->str_poly_b * m) / (m * m);
}

static HapticFeedbackType haptic_feedback_get_type(
    const HapticFeedback *hf, const State *state, const MotorData *md, const AlertTracker *at
) {
    // TODO: Ideally we don't even do pushback in handtest, as it can be confusing
    if (state->state != STATE_RUNNING || state->mode == MODE_HANDTEST) {
        return HAPTIC_FEEDBACK_NONE;
    }

    if (at->fatal_error) {
        return HAPTIC_FEEDBACK_ERROR_FATAL;
    }

    switch (state->sat) {
    case SAT_PB_DUTY:
        if (md->duty_cycle.value > hf->duty_solid_threshold) {
            return HAPTIC_FEEDBACK_DUTY_CONTINUOUS;
        } else {
            return HAPTIC_FEEDBACK_DUTY_SPEED;
        }
    case SAT_PB_SPEED:
        return HAPTIC_FEEDBACK_DUTY_SPEED;
    case SAT_PB_TEMPERATURE:
        return HAPTIC_FEEDBACK_ERROR_TEMPERATURE;
    case SAT_PB_LOW_VOLTAGE:
    case SAT_PB_HIGH_VOLTAGE:
    case SAT_PB_ERROR:
        return HAPTIC_FEEDBACK_ERROR_VOLTAGE;
    default:
        break;
    }

    const float current_threshold = clampf(hf->cfg->current_threshold, 0.0f, 1.0f);
    if (current_threshold > 0.0f && motor_data_get_current_saturation(md) > current_threshold) {
        return HAPTIC_FEEDBACK_DUTY_CONTINUOUS;
    }

    return HAPTIC_FEEDBACK_NONE;
}

// Returns the number of "beats" per period of a given tone. Tones are played
// on even beats and if there are more than two beats, the last beat is
// skipped, giving a certain number of "beeps" followed by a pause.
static uint8_t get_beats(HapticFeedbackType type) {
    switch (type
    ) {  // GCOVR_EXCL_BR_LINE: the caller excludes NONE; every playable type is covered.
    case HAPTIC_FEEDBACK_DUTY_SPEED:
        return 2;
    case HAPTIC_FEEDBACK_DUTY_CONTINUOUS:
        return 0;
    case HAPTIC_FEEDBACK_ERROR_TEMPERATURE:
        return 6;
    case HAPTIC_FEEDBACK_ERROR_VOLTAGE:
        return 8;
    case HAPTIC_FEEDBACK_ERROR_FATAL:
        return 10;
    case HAPTIC_FEEDBACK_NONE:
        break;
    }

    return 0;  // GCOVR_EXCL_LINE: NONE is rejected by the caller; playable types return above.
}

static const CfgHapticTone *get_haptic_tone(const HapticFeedback *hf) {
    switch (hf->type_playing) {  // GCOVR_EXCL_BR_LINE: called only while a playable type is active.
    case HAPTIC_FEEDBACK_DUTY_SPEED:
    case HAPTIC_FEEDBACK_DUTY_CONTINUOUS:
        return &hf->cfg->duty;
    case HAPTIC_FEEDBACK_ERROR_TEMPERATURE:
    case HAPTIC_FEEDBACK_ERROR_VOLTAGE:
    case HAPTIC_FEEDBACK_ERROR_FATAL:
        return &hf->cfg->error;
    case HAPTIC_FEEDBACK_NONE:
        break;
    }

    return 0;
}

static inline float strength_scale(const HapticFeedback *hf, float speed) {
    return clampf(
        hf->min_strength + hf->str_poly_b * speed + hf->str_poly_c * speed * speed, 0.0f, 1.0f
    );
}

static inline void foc_play_tone(int channel, float freq, float voltage) {
    if (!VESC_IF->foc_play_tone) {  // GCOVR_EXCL_BR_LINE: NULL and non-NULL are covered; gcov adds
                                    // indirect-call edges.
        return;
    }

    VESC_IF->foc_play_tone(channel, freq, voltage);
}

void haptic_feedback_update(
    HapticFeedback *hf,
    MotorControl *mc,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
) {
    HapticFeedbackType type_to_play = haptic_feedback_get_type(hf, state, md, at);

    if (type_to_play != hf->type_playing && hf->can_change_type) {
        hf->type_playing = type_to_play;
        timer_refresh(time, &hf->tone_timer);
    }

    bool should_be_playing = false;
    if (hf->type_playing != HAPTIC_FEEDBACK_NONE) {
        uint8_t beats = get_beats(hf->type_playing);
        if (beats == 0) {
            should_be_playing = true;
            hf->can_change_type = true;
        } else {
            float period = TONE_LENGTH * beats;
            float tone_time = fmodf(timer_age(time, hf->tone_timer), period);
            uint8_t beat = floorf(tone_time / TONE_LENGTH);
            uint8_t off_beat = beats > 2 ? beats - 2 : 0;

            should_be_playing = beat % 2 == 0 && (off_beat == 0 || beat != off_beat);
            // Only allow changing type (another pattern or stop alerting)
            // if we just finished a period
            hf->can_change_type = !hf->is_playing && beat == 0;
        }
    } else {
        hf->can_change_type = true;
    }

    if (hf->is_playing && !should_be_playing) {
        foc_play_tone(0, 1, 0.0f);
        motor_control_stop_tone(mc);
        hf->is_playing = false;
    } else if (should_be_playing) {
        const CfgHapticTone *tone = get_haptic_tone(hf);
        const float scale = strength_scale(hf, fabsf(md->speed));
        const float audible_strength = clampf(tone->strength, 0.0f, AUDIBLE_STRENGTH_MAX);
        if (audible_strength > 0.0f) {
            foc_play_tone(
                0,
                clampf(tone->frequency, AUDIBLE_FREQUENCY_MIN, AUDIBLE_FREQUENCY_MAX),
                audible_strength * scale
            );
        } else if (hf->is_playing) {
            foc_play_tone(0, 1, 0.0f);
        }

        const float vibrate_strength =
            clampf(hf->cfg->vibrate.strength, 0.0f, VIBRATE_STRENGTH_MAX);
        if (vibrate_strength > 0.0f) {
            motor_control_play_tone(
                mc,
                min(max(hf->cfg->vibrate.frequency, VIBRATE_FREQUENCY_MIN), VIBRATE_FREQUENCY_MAX),
                vibrate_strength * scale
            );
        } else if (hf->is_playing) {
            motor_control_stop_tone(mc);
        }

        hf->is_playing = true;
    }
}
