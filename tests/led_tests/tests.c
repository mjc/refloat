#include "footpad_sensor.h"
#include "leds.h"
#include "lib/utils.h"
#include "motor_data.h"
#include "state.h"
#include "vesc_if_fake.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../test_runner.h"

void led_driver_fake_reset(void);
void led_driver_fake_set_setup_result(bool result);
size_t led_driver_fake_setup_calls(void);
size_t led_driver_fake_paint_calls(void);
size_t led_driver_fake_destroy_calls(void);

static LedBar solid_bar(float brightness, LedColor color) {
    LedBar bar = {
        .brightness = brightness,
        .color1 = color,
        .color2 = COLOR_BLACK,
        .mode = LED_ANIM_SOLID,
        .speed = 1.0f,
    };
    return bar;
}

static CfgLeds default_leds_cfg(void) {
    CfgLeds cfg = {
        .on = true,
        .headlights_on = true,
        .headlights_transition = LED_TRANS_FADE,
        .direction_transition = LED_TRANS_FADE,
        .lights_off_when_lifted = true,
        .status_on_front_when_lifted = true,
        .headlights = solid_bar(0.8f, COLOR_WHITE_RGB),
        .taillights = solid_bar(0.6f, COLOR_RED),
        .front = solid_bar(0.5f, COLOR_BLUE),
        .rear = solid_bar(0.4f, COLOR_GREEN),
        .status =
            {
                .idle_timeout = 2,
                .motor_utilization_threshold = 0.5f,
                .red_bar_percentage = 0.15f,
                .show_sensors_while_running = false,
                .brightness_headlights_on = 0.25f,
                .brightness_headlights_off = 0.1f,
            },
        .status_idle = solid_bar(0.05f, COLOR_LAVENDER),
    };
    return cfg;
}

static CfgHwLeds default_hw_cfg(void) {
    CfgHwLeds hw = {
        .mode = LED_MODE_INTERNAL,
        .pin = LED_PIN_B6,
        .pin_config = LED_PIN_CFG_PULLUP_TO_5V,
        .status =
            {
                .order = LED_STRIP_ORDER_1ST,
                .count = 4,
                .color_order = LED_COLOR_GRB,
                .reverse = false,
            },
        .front =
            {
                .order = LED_STRIP_ORDER_2ND,
                .count = 3,
                .color_order = LED_COLOR_RGB,
                .reverse = false,
            },
        .rear =
            {
                .order = LED_STRIP_ORDER_3RD,
                .count = 3,
                .color_order = LED_COLOR_RGB,
                .reverse = true,
            },
    };
    return hw;
}

static void reset_fakes(float seconds) {
    vesc_if_fake_reset();
    led_driver_fake_reset();
    vesc_if_fake_set_seconds(seconds);
    vesc_if_fake_set_imu(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
}

typedef struct {
    CfgLeds cfg;
    CfgHwLeds hw;
    Leds leds;
    State state;
    MotorData motor;
} LedsFixture;

static LedsFixture leds_fixture_prepare(float seconds) {
    reset_fakes(seconds);
    return (LedsFixture){
        .cfg = default_leds_cfg(),
        .hw = default_hw_cfg(),
    };
}

static void leds_fixture_start(LedsFixture *fixture, RunState initial_state) {
    leds_init(&fixture->leds);
    leds_setup(&fixture->leds, &fixture->hw, &fixture->cfg);
    state_init(&fixture->state);
    fixture->state.state = initial_state;
    memset(&fixture->motor, 0, sizeof(fixture->motor));
}

static void leds_fixture_setup(LedsFixture *fixture, float seconds, RunState initial_state) {
    *fixture = leds_fixture_prepare(seconds);
    leds_fixture_start(fixture, initial_state);
}

static void leds_fixture_destroy(LedsFixture *fixture) {
    leds_destroy(&fixture->leds);
}

static void update_leds_at(LedsFixture *fixture, FootpadSensorState fs_state, float seconds) {
    vesc_if_fake_set_seconds(seconds);
    leds_update(&fixture->leds, &fixture->state, &fixture->motor, fs_state);
}
// grouped test implementations live in the included feature files below.

#include "animation_modes.tests.c"
#include "setup_runtime.tests.c"
#include "status.tests.c"
#include "transitions.tests.c"
int main(void) {
    const TestCase tests[] = {
        TEST_CASE(
            "leds setup configures internal strips and runtime defaults",
            test_leds_setup_configures_internal_strips_and_runtime_defaults
        ),
        TEST_CASE(
            "leds runtime overrides survive reconfigure",
            test_leds_runtime_overrides_survive_reconfigure
        ),
        TEST_CASE(
            "leds update paints and fades running lights",
            test_leds_update_paints_and_fades_running_lights
        ),
        TEST_CASE(
            "leds update is noop without internal led data",
            test_leds_update_is_noop_without_internal_led_data
        ),
        TEST_CASE(
            "leds setup frees buffer when driver setup fails",
            test_leds_setup_frees_buffer_when_driver_setup_fails
        ),
        TEST_CASE(
            "leds setup respects strip order and metadata",
            test_leds_setup_respects_strip_order_and_metadata
        ),
        TEST_CASE(
            "leds setup rejects strip count above internal limit",
            test_leds_setup_rejects_strip_count_above_internal_limit
        ),
        TEST_CASE(
            "leds status confirm respects animation window",
            test_leds_status_confirm_respects_animation_window
        ),
        TEST_CASE(
            "leds startup state updates timestamp without painting",
            test_leds_startup_state_updates_timestamp_without_painting
        ),
        TEST_CASE(
            "leds disabled state paints disabled animation and resets fade",
            test_leds_disabled_state_paints_disabled_animation_and_resets_fade
        ),
        TEST_CASE(
            "leds lifted ready board blends status onto front strip",
            test_leds_lifted_ready_board_blends_status_onto_front_strip
        ),
        TEST_CASE(
            "leds running state hides sensor indicators when configured",
            test_leds_running_state_hides_sensor_indicators_when_configured
        ),
        TEST_CASE(
            "leds running entry direction and headlight transition",
            test_leds_running_entry_direction_and_headlight_transition
        ),
        TEST_CASE(
            "leds running transition repaints each tick",
            test_leds_running_transition_repaints_each_tick
        ),
        TEST_CASE(
            "leds ignores nonfinite distance for direction transition",
            test_leds_ignores_nonfinite_distance_for_direction_transition
        ),
        TEST_CASE(
            "leds cipher transition golden colors", test_leds_cipher_transition_golden_colors
        ),
        TEST_CASE(
            "leds invalid transition falls back to fade",
            test_leds_invalid_transition_falls_back_to_fade
        ),
        TEST_CASE("leds render valid runtime states", test_leds_render_valid_runtime_states),
        TEST_CASE(
            "leds setup rejects duplicate strip orders",
            test_leds_setup_rejects_duplicate_strip_orders
        ),
        TEST_CASE(
            "leds setup rejects out of range strip orders",
            test_leds_setup_rejects_out_of_range_strip_orders
        ),
        TEST_CASE(
            "leds preserve configured strip layouts", test_leds_preserve_configured_strip_layouts
        ),
        TEST_CASE(
            "leds configure internal strip layout", test_leds_configure_internal_strip_layout
        ),
        TEST_CASE(
            "leds runtime enabled override fades lights off",
            test_leds_runtime_enabled_override_fades_lights_off
        ),
        TEST_CASE(
            "leds headlights runtime override uses off status brightness",
            test_leds_headlights_runtime_override_uses_off_status_brightness
        ),
        TEST_CASE(
            "leds status idle blend starts after timeout and resets on sensor",
            test_leds_status_idle_blend_starts_after_timeout_and_resets_on_sensor
        ),
        TEST_CASE("leds blend front idle status", test_leds_blend_front_idle_status),
        TEST_CASE("leds render battery boundaries", test_leds_render_battery_boundaries),
        TEST_CASE("leds retain utilization hysteresis", test_leds_retain_utilization_hysteresis),
        TEST_CASE(
            "leds animation modes render without out of bounds writes",
            test_leds_animation_modes_render_without_oob_writes
        ),
        TEST_CASE(
            "leds invalid animation and colors render black",
            test_leds_invalid_animation_and_colors_render_black
        ),
        TEST_CASE(
            "leds rainbow cycle handles long uptime", test_leds_rainbow_cycle_handles_long_uptime
        ),
        TEST_CASE(
            "leds animation time survives VESC tick wrap",
            test_leds_animation_time_survives_vesc_tick_wrap
        ),
        TEST_CASE(
            "leds negative animation speed is stopped",
            test_leds_negative_animation_speed_is_stopped
        ),
        TEST_CASE(
            "leds bound serialized animation and status config",
            test_leds_bounds_serialized_animation_and_status_config
        ),
    };

    RUN_TEST_SUITE("leds summary", tests);
}
