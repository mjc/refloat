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
    return (LedsFixture) {
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

static bool test_leds_setup_configures_internal_strips_and_runtime_defaults(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 4.0f, STATE_READY);

    EXPECT_TRUE(fixture.leds.led_data != NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);
    EXPECT_TRUE(fixture.leds.status_strip.data == fixture.leds.led_data);
    EXPECT_TRUE(fixture.leds.front_strip.data == fixture.leds.led_data + fixture.hw.status.count);
    EXPECT_TRUE(fixture.leds.rear_strip.data ==
        fixture.leds.led_data + fixture.hw.status.count + fixture.hw.front.count);
    EXPECT_TRUE(fixture.leds.status_strip.length == fixture.hw.status.count);
    EXPECT_TRUE(fixture.leds.front_strip.length == fixture.hw.front.count);
    EXPECT_TRUE(fixture.leds.rear_strip.length == fixture.hw.rear.count);
    EXPECT_FLOAT_NEAR(fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on);
    EXPECT_FLOAT_NEAR(fixture.leds.front_strip.brightness, fixture.cfg.front.brightness);
    EXPECT_FLOAT_NEAR(fixture.leds.rear_strip.brightness, fixture.cfg.rear.brightness);
    EXPECT_TRUE(leds_get_runtime_status(&fixture.leds)->enabled);
    EXPECT_TRUE(leds_get_runtime_status(&fixture.leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 4.0f);

    leds_fixture_destroy(&fixture);
    EXPECT_TRUE(fixture.leds.led_data == NULL);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);
    EXPECT_TRUE(led_driver_fake_destroy_calls() == 1);
    return true;
}

static bool test_leds_runtime_overrides_survive_reconfigure(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 2.0f, STATE_READY);

    leds_set_enabled(&fixture.leds, false);
    leds_set_headlights_enabled(&fixture.leds, false);
    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->enabled);
    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->headlights_enabled);

    fixture.cfg.on = true;
    fixture.cfg.headlights_on = true;
    vesc_if_fake_set_seconds(8.0f);
    leds_configure(&fixture.leds, &fixture.cfg);

    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->enabled);
    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 8.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 8.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_update_paints_and_fades_running_lights(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 1.0f, STATE_READY);
    update_leds_at(&fixture, FS_BOTH, 1.1f);

    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);
    EXPECT_FLOAT_NEAR(fixture.leds.last_updated, 1.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 1.0f / 3.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 1.0f / 3.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_update_is_noop_without_internal_led_data(void) {
    LedsFixture fixture = leds_fixture_prepare(1.0f);
    fixture.hw.mode = LED_MODE_OFF;
    leds_fixture_start(&fixture, STATE_READY);
    leds_update(&fixture.leds, &fixture.state, &fixture.motor, FS_BOTH);
    leds_status_confirm(&fixture.leds);

    EXPECT_TRUE(fixture.leds.led_data == NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 0);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 0);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 0);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_setup_frees_buffer_when_driver_setup_fails(void) {
    LedsFixture fixture = leds_fixture_prepare(1.0f);
    led_driver_fake_set_setup_result(false);
    leds_fixture_start(&fixture, STATE_READY);

    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);
    EXPECT_TRUE(fixture.leds.led_data == NULL);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_setup_respects_strip_order_and_metadata(void) {
    LedsFixture fixture = leds_fixture_prepare(13.0f);
    fixture.hw.status.order = LED_STRIP_ORDER_3RD;
    fixture.hw.status.count = 2;
    fixture.hw.status.color_order = LED_COLOR_RGB;
    fixture.hw.status.reverse = true;
    fixture.hw.front.order = LED_STRIP_ORDER_1ST;
    fixture.hw.front.count = 4;
    fixture.hw.front.color_order = LED_COLOR_GRB;
    fixture.hw.front.reverse = true;
    fixture.hw.rear.order = LED_STRIP_ORDER_2ND;
    fixture.hw.rear.count = 3;
    fixture.hw.rear.color_order = LED_COLOR_WRGB;
    fixture.hw.rear.reverse = false;
    leds_fixture_start(&fixture, STATE_READY);

    EXPECT_TRUE(fixture.leds.led_data != NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);

    EXPECT_TRUE(fixture.leds.front_strip.data == fixture.leds.led_data);
    EXPECT_TRUE(fixture.leds.rear_strip.data == fixture.leds.led_data + fixture.hw.front.count);
    EXPECT_TRUE(fixture.leds.status_strip.data ==
        fixture.leds.led_data + fixture.hw.front.count + fixture.hw.rear.count);

    EXPECT_TRUE(fixture.leds.front_strip.length == fixture.hw.front.count);
    EXPECT_TRUE(fixture.leds.rear_strip.length == fixture.hw.rear.count);
    EXPECT_TRUE(fixture.leds.status_strip.length == fixture.hw.status.count);
    EXPECT_TRUE(fixture.leds.front_strip.color_order == LED_COLOR_GRB);
    EXPECT_TRUE(fixture.leds.rear_strip.color_order == LED_COLOR_WRGB);
    EXPECT_TRUE(fixture.leds.status_strip.color_order == LED_COLOR_RGB);
    EXPECT_TRUE(fixture.leds.front_strip.reverse);
    EXPECT_TRUE(!fixture.leds.rear_strip.reverse);
    EXPECT_TRUE(fixture.leds.status_strip.reverse);

    for (uint8_t i = 0; i < fixture.hw.front.count + fixture.hw.rear.count + fixture.hw.status.count; ++i) {
        EXPECT_TRUE(fixture.leds.led_data[i] == 0u);
    }

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_status_confirm_respects_animation_window(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 10.0f, STATE_READY);

    leds_status_confirm(&fixture.leds);
    EXPECT_FLOAT_NEAR(fixture.leds.confirm_animation_start, 10.0f);

    vesc_if_fake_set_seconds(10.5f);
    leds_status_confirm(&fixture.leds);
    EXPECT_FLOAT_NEAR(fixture.leds.confirm_animation_start, 10.0f);

    vesc_if_fake_set_seconds(11.0f);
    leds_status_confirm(&fixture.leds);
    EXPECT_FLOAT_NEAR(fixture.leds.confirm_animation_start, 11.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_startup_state_updates_timestamp_without_painting(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 3.0f, STATE_STARTUP);

    update_leds_at(&fixture, FS_BOTH, 3.5f);

    EXPECT_FLOAT_NEAR(fixture.leds.last_updated, 3.5f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 0);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_disabled_state_paints_disabled_animation_and_resets_fade(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 5.0f, STATE_READY);
    update_leds_at(&fixture, FS_NONE, 5.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.1f);

    fixture.state.state = STATE_DISABLED;
    update_leds_at(&fixture, FS_NONE, 5.2f);

    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_FLOAT_NEAR(fixture.leds.animation_start, 5.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 5.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 5.2f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_lifted_ready_board_blends_status_onto_front_strip(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 6.0f, STATE_READY);

    vesc_if_fake_set_imu(deg2rad(65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 6.1f);

    EXPECT_TRUE(fixture.leds.board_is_upright);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_blend, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.front_strip.brightness, fixture.cfg.front.brightness - 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.rear_strip.brightness, fixture.cfg.rear.brightness - 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 6.1f);

    vesc_if_fake_set_imu(deg2rad(45.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 6.2f);

    EXPECT_TRUE(!fixture.leds.board_is_upright);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_blend, 0.9f);
    EXPECT_FLOAT_NEAR(fixture.leds.animation_start, 6.2f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_running_state_hides_sensor_indicators_when_configured(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 7.0f, STATE_READY);
    update_leds_at(&fixture, FS_BOTH, 7.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 1.0f / 3.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 1.0f / 3.0f);

    fixture.state.state = STATE_RUNNING;
    update_leds_at(&fixture, FS_BOTH, 7.2f);

    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.split_distance, 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_running_entry_direction_and_headlight_transition(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 15.0f, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 15.1f);
    EXPECT_TRUE(!fixture.leds.headlights_on);
    EXPECT_TRUE(fixture.leds.direction_forward);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-3.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 2.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 15.2f);

    EXPECT_TRUE(!fixture.leds.direction_forward);
    EXPECT_FLOAT_NEAR(fixture.leds.dir_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_time, 15.2f);
    EXPECT_TRUE(!fixture.leds.headlights_on);
    EXPECT_TRUE(fixture.leds.front_time_target == &fixture.cfg.taillights);
    EXPECT_TRUE(fixture.leds.rear_time_target == &fixture.cfg.headlights);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 15.8f);
    EXPECT_TRUE(!fixture.leds.headlights_on);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, 0.2f);

    vesc_if_fake_set_motor_telemetry(0, 0, 4.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 16.3f);
    EXPECT_TRUE(fixture.leds.headlights_on);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_time, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.split_distance, 4.0f);
    EXPECT_TRUE(fixture.leds.front_bar == &fixture.cfg.taillights);
    EXPECT_TRUE(fixture.leds.rear_bar == &fixture.cfg.headlights);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_running_transition_repaints_each_tick(void) {
    reset_fakes(20.0f);

    LedsFixture fixture = leds_fixture_prepare(20.0f);
    leds_fixture_start(&fixture, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 20.1f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-4.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 3.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 20.2f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_FLOAT_NEAR(fixture.leds.dir_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, -1.0f);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.4f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 20.4f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 3);
    EXPECT_TRUE(fixture.leds.headlights_trans.split > -1.0f);
    EXPECT_TRUE(fixture.leds.headlights_trans.split < 1.0f);

    float split_after_first_transition_tick = fixture.leds.headlights_trans.split;
    vesc_if_fake_set_motor_telemetry(0, 0, 3.8f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 20.6f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 4);
    EXPECT_TRUE(fixture.leds.headlights_trans.split > split_after_first_transition_tick);
    EXPECT_TRUE(fixture.leds.headlights_trans.split < 1.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_cipher_transition_golden_colors(void) {
    reset_fakes(30.0f);

    LedsFixture fixture = leds_fixture_prepare(30.0f);
    fixture.cfg.headlights_transition = LED_TRANS_CIPHER;
    fixture.cfg.direction_transition = LED_TRANS_CIPHER;
    fixture.cfg.front = solid_bar(1.0f, COLOR_BLUE);
    fixture.cfg.rear = solid_bar(1.0f, COLOR_GREEN);
    fixture.cfg.headlights = solid_bar(1.0f, COLOR_WHITE_RGB);
    fixture.cfg.taillights = solid_bar(1.0f, COLOR_RED);
    fixture.cfg.status.idle_timeout = 0;
    leds_fixture_start(&fixture, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 30.1f);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-4.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 2.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 30.2f);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 30.6f);

    EXPECT_TRUE(fixture.leds.front_strip.length == 3);
    EXPECT_TRUE(fixture.leds.rear_strip.length == 3);
    EXPECT_TRUE(fixture.leds.front_strip.data[0] == 0x00000000u);
    EXPECT_TRUE(fixture.leds.front_strip.data[1] == 0x00274d2cu);
    EXPECT_TRUE(fixture.leds.front_strip.data[2] == 0x00264c23u);
    EXPECT_TRUE(fixture.leds.rear_strip.data[0] == 0x00264c23u);
    EXPECT_TRUE(fixture.leds.rear_strip.data[1] == 0x00274d2cu);
    EXPECT_TRUE(fixture.leds.rear_strip.data[2] == 0x00000000u);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 3);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_setup_rejects_front_rear_count_over_limit(void) {
    LedsFixture fixture = leds_fixture_prepare(1.0f);
    fixture.hw.front.count = LEDS_FRONT_AND_REAR_COUNT_MAX;
    fixture.hw.rear.count = 1;
    leds_fixture_start(&fixture, STATE_READY);

    EXPECT_TRUE(fixture.leds.led_data == NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 0);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 0);
    EXPECT_TRUE(fixture.leds.front_strip.length == LEDS_FRONT_AND_REAR_COUNT_MAX);
    EXPECT_TRUE(fixture.leds.rear_strip.length == 1);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_runtime_enabled_override_fades_lights_off(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 8.0f, STATE_READY);
    update_leds_at(&fixture, FS_NONE, 8.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.1f);

    leds_set_enabled(&fixture.leds, false);
    update_leds_at(&fixture, FS_NONE, 8.2f);

    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->enabled);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_headlights_runtime_override_uses_off_status_brightness(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 9.0f, STATE_READY);
    EXPECT_FLOAT_NEAR(fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on);

    leds_set_headlights_enabled(&fixture.leds, false);
    update_leds_at(&fixture, FS_NONE, 9.1f);

    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on - 0.1f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_status_idle_blend_starts_after_timeout_and_resets_on_sensor(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 12.0f, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 12.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 12.1f);

    update_leds_at(&fixture, FS_NONE, 14.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_blend, 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_animation_start, 14.2f);

    update_leds_at(&fixture, FS_LEFT, 14.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 14.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_blend, 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_status_on_front_idle_blend_edges(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 22.0f, STATE_READY);

    vesc_if_fake_set_imu(deg2rad(65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 22.1f);
    EXPECT_TRUE(fixture.leds.board_is_upright);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_blend, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 22.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.0f);

    update_leds_at(&fixture, FS_NONE, 25.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.1f);

    update_leds_at(&fixture, FS_RIGHT, 25.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 25.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.0f);

    fixture.cfg.lights_off_when_lifted = false;
    update_leds_at(&fixture, FS_NONE, 28.4f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_status_battery_bar_edges(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 20.0f, STATE_READY);

    vesc_if_fake_set_battery_level(0.5f);
    update_leds_at(&fixture, FS_NONE, 20.1f);

    EXPECT_TRUE(fixture.leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[1] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[0] == fixture.leds.status_strip.data[1]);
    EXPECT_TRUE(fixture.leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[3] == 0u);

    uint32_t mid_battery_first_led = fixture.leds.status_strip.data[0];
    vesc_if_fake_set_battery_level(0.0f);
    update_leds_at(&fixture, FS_NONE, 20.2f);

    EXPECT_TRUE(fixture.leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[0] != mid_battery_first_led);
    EXPECT_TRUE(fixture.leds.status_strip.data[1] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[3] == 0u);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_status_utilization_hysteresis_edges(void) {
    reset_fakes(18.0f);
    LedsFixture fixture = leds_fixture_prepare(18.0f);
    fixture.cfg.status.motor_utilization_threshold = 0.5f;
    leds_fixture_start(&fixture, STATE_RUNNING);

    fixture.motor.duty_cycle.value = 0.54f;
    update_leds_at(&fixture, FS_NONE, 18.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.motor.duty_cycle.value = 0.405f;
    update_leds_at(&fixture, FS_NONE, 18.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.motor.duty_cycle.value = 0.35f;
    update_leds_at(&fixture, FS_NONE, 18.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 0.0f);

    fixture.motor.duty_cycle.value = 0.0f;
    fixture.motor.motor_current_saturation = 0.9f;
    update_leds_at(&fixture, FS_NONE, 18.4f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.motor.motor_current_saturation = 0.0f;
    fixture.motor.battery_current_saturation = 0.95f;
    update_leds_at(&fixture, FS_NONE, 18.5f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 10.0f / LEDS_REFRESH_RATE);

    leds_fixture_destroy(&fixture);
    return true;
}
int main(void) {
    const TestCase tests[] = {
        TEST_CASE("leds setup configures internal strips and runtime defaults", test_leds_setup_configures_internal_strips_and_runtime_defaults),
        TEST_CASE("leds runtime overrides survive reconfigure", test_leds_runtime_overrides_survive_reconfigure),
        TEST_CASE("leds update paints and fades running lights", test_leds_update_paints_and_fades_running_lights),
        TEST_CASE("leds update is noop without internal led data", test_leds_update_is_noop_without_internal_led_data),
        TEST_CASE("leds setup frees buffer when driver setup fails", test_leds_setup_frees_buffer_when_driver_setup_fails),
        TEST_CASE("leds setup respects strip order and metadata", test_leds_setup_respects_strip_order_and_metadata),
        TEST_CASE("leds status confirm respects animation window", test_leds_status_confirm_respects_animation_window),
        TEST_CASE("leds startup state updates timestamp without painting", test_leds_startup_state_updates_timestamp_without_painting),
        TEST_CASE("leds disabled state paints disabled animation and resets fade", test_leds_disabled_state_paints_disabled_animation_and_resets_fade),
        TEST_CASE("leds lifted ready board blends status onto front strip", test_leds_lifted_ready_board_blends_status_onto_front_strip),
        TEST_CASE("leds running state hides sensor indicators when configured", test_leds_running_state_hides_sensor_indicators_when_configured),
        TEST_CASE("leds running entry direction and headlight transition", test_leds_running_entry_direction_and_headlight_transition),
        TEST_CASE("leds running transition repaints each tick", test_leds_running_transition_repaints_each_tick),
        TEST_CASE("leds cipher transition golden colors", test_leds_cipher_transition_golden_colors),
        TEST_CASE("leds setup rejects front rear count over limit", test_leds_setup_rejects_front_rear_count_over_limit),
        TEST_CASE("leds runtime enabled override fades lights off", test_leds_runtime_enabled_override_fades_lights_off),
        TEST_CASE("leds headlights runtime override uses off status brightness", test_leds_headlights_runtime_override_uses_off_status_brightness),
        TEST_CASE("leds status idle blend starts after timeout and resets on sensor", test_leds_status_idle_blend_starts_after_timeout_and_resets_on_sensor),
        TEST_CASE("leds status on front idle blend edges", test_leds_status_on_front_idle_blend_edges),
        TEST_CASE("leds status battery bar edges", test_leds_status_battery_bar_edges),
        TEST_CASE("leds status utilization hysteresis edges", test_leds_status_utilization_hysteresis_edges),
    };

    RUN_TEST_SUITE("leds summary", tests);
}
