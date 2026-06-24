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

#define LEDS_DEFAULT_FIXTURE(seconds) \
    reset_fakes(seconds); \
    CfgLeds cfg = default_leds_cfg(); \
    CfgHwLeds hw = default_hw_cfg()

#define LEDS_SETUP_FROM_CONFIG() \
    Leds leds; \
    leds_init(&leds); \
    leds_setup(&leds, &hw, &cfg)

#define LEDS_UPDATE_FROM_CONFIG(initial_state) \
    LEDS_SETUP_FROM_CONFIG(); \
    State state; \
    MotorData motor; \
    memset(&motor, 0, sizeof(motor)); \
    state_init(&state); \
    state.state = (initial_state)

#define LEDS_SETUP_FIXTURE(seconds) \
    LEDS_DEFAULT_FIXTURE(seconds); \
    LEDS_SETUP_FROM_CONFIG()

#define LEDS_UPDATE_FIXTURE(seconds, initial_state) \
    LEDS_DEFAULT_FIXTURE(seconds); \
    LEDS_UPDATE_FROM_CONFIG(initial_state)

static void update_leds_at(
    Leds *leds, State *state, MotorData *motor, FootpadSensorState fs_state, float seconds
) {
    vesc_if_fake_set_seconds(seconds);
    leds_update(leds, state, motor, fs_state);
}

static bool test_leds_setup_configures_internal_strips_and_runtime_defaults(void) {
    LEDS_SETUP_FIXTURE(4.0f);

    EXPECT_TRUE(leds.led_data != NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);
    EXPECT_TRUE(leds.status_strip.data == leds.led_data);
    EXPECT_TRUE(leds.front_strip.data == leds.led_data + hw.status.count);
    EXPECT_TRUE(leds.rear_strip.data == leds.led_data + hw.status.count + hw.front.count);
    EXPECT_TRUE(leds.status_strip.length == hw.status.count);
    EXPECT_TRUE(leds.front_strip.length == hw.front.count);
    EXPECT_TRUE(leds.rear_strip.length == hw.rear.count);
    EXPECT_FLOAT_NEAR(leds.status_strip.brightness, cfg.status.brightness_headlights_on);
    EXPECT_FLOAT_NEAR(leds.front_strip.brightness, cfg.front.brightness);
    EXPECT_FLOAT_NEAR(leds.rear_strip.brightness, cfg.rear.brightness);
    EXPECT_TRUE(leds_get_runtime_status(&leds)->enabled);
    EXPECT_TRUE(leds_get_runtime_status(&leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(leds.status_idle_time, 4.0f);

    leds_destroy(&leds);
    EXPECT_TRUE(leds.led_data == NULL);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);
    EXPECT_TRUE(led_driver_fake_destroy_calls() == 1);
    return true;
}

static bool test_leds_runtime_overrides_survive_reconfigure(void) {
    LEDS_SETUP_FIXTURE(2.0f);

    leds_set_enabled(&leds, false);
    leds_set_headlights_enabled(&leds, false);
    EXPECT_TRUE(!leds_get_runtime_status(&leds)->enabled);
    EXPECT_TRUE(!leds_get_runtime_status(&leds)->headlights_enabled);

    cfg.on = true;
    cfg.headlights_on = true;
    vesc_if_fake_set_seconds(8.0f);
    leds_configure(&leds, &cfg);

    EXPECT_TRUE(!leds_get_runtime_status(&leds)->enabled);
    EXPECT_TRUE(!leds_get_runtime_status(&leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(leds.status_idle_time, 8.0f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_time, 8.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_update_paints_and_fades_running_lights(void) {
    LEDS_UPDATE_FIXTURE(1.0f, STATE_READY);
    update_leds_at(&leds, &state, &motor, FS_BOTH, 1.1f);

    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);
    EXPECT_FLOAT_NEAR(leds.last_updated, 1.1f);
    EXPECT_FLOAT_NEAR(leds.on_off_fade, 0.1f);
    EXPECT_FLOAT_NEAR(leds.left_sensor, 1.0f / 3.0f);
    EXPECT_FLOAT_NEAR(leds.right_sensor, 1.0f / 3.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_update_is_noop_without_internal_led_data(void) {
    LEDS_DEFAULT_FIXTURE(1.0f);
    hw.mode = LED_MODE_OFF;
    LEDS_UPDATE_FROM_CONFIG(STATE_READY);
    leds_update(&leds, &state, &motor, FS_BOTH);
    leds_status_confirm(&leds);

    EXPECT_TRUE(leds.led_data == NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 0);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 0);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 0);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_setup_frees_buffer_when_driver_setup_fails(void) {
    reset_fakes(1.0f);
    led_driver_fake_set_setup_result(false);
    CfgLeds cfg = default_leds_cfg();
    CfgHwLeds hw = default_hw_cfg();
    LEDS_SETUP_FROM_CONFIG();

    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);
    EXPECT_TRUE(leds.led_data == NULL);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_setup_respects_strip_order_and_metadata(void) {
    LEDS_DEFAULT_FIXTURE(13.0f);
    hw.status.order = LED_STRIP_ORDER_3RD;
    hw.status.count = 2;
    hw.status.color_order = LED_COLOR_RGB;
    hw.status.reverse = true;
    hw.front.order = LED_STRIP_ORDER_1ST;
    hw.front.count = 4;
    hw.front.color_order = LED_COLOR_GRB;
    hw.front.reverse = true;
    hw.rear.order = LED_STRIP_ORDER_2ND;
    hw.rear.count = 3;
    hw.rear.color_order = LED_COLOR_WRGB;
    hw.rear.reverse = false;

    LEDS_SETUP_FROM_CONFIG();

    EXPECT_TRUE(leds.led_data != NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);

    EXPECT_TRUE(leds.front_strip.data == leds.led_data);
    EXPECT_TRUE(leds.rear_strip.data == leds.led_data + hw.front.count);
    EXPECT_TRUE(leds.status_strip.data == leds.led_data + hw.front.count + hw.rear.count);

    EXPECT_TRUE(leds.front_strip.length == hw.front.count);
    EXPECT_TRUE(leds.rear_strip.length == hw.rear.count);
    EXPECT_TRUE(leds.status_strip.length == hw.status.count);
    EXPECT_TRUE(leds.front_strip.color_order == LED_COLOR_GRB);
    EXPECT_TRUE(leds.rear_strip.color_order == LED_COLOR_WRGB);
    EXPECT_TRUE(leds.status_strip.color_order == LED_COLOR_RGB);
    EXPECT_TRUE(leds.front_strip.reverse);
    EXPECT_TRUE(!leds.rear_strip.reverse);
    EXPECT_TRUE(leds.status_strip.reverse);

    for (uint8_t i = 0; i < hw.front.count + hw.rear.count + hw.status.count; ++i) {
        EXPECT_TRUE(leds.led_data[i] == 0u);
    }

    leds_destroy(&leds);
    return true;
}

static bool test_leds_status_confirm_respects_animation_window(void) {
    LEDS_SETUP_FIXTURE(10.0f);

    leds_status_confirm(&leds);
    EXPECT_FLOAT_NEAR(leds.confirm_animation_start, 10.0f);

    vesc_if_fake_set_seconds(10.5f);
    leds_status_confirm(&leds);
    EXPECT_FLOAT_NEAR(leds.confirm_animation_start, 10.0f);

    vesc_if_fake_set_seconds(11.0f);
    leds_status_confirm(&leds);
    EXPECT_FLOAT_NEAR(leds.confirm_animation_start, 11.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_startup_state_updates_timestamp_without_painting(void) {
    LEDS_UPDATE_FIXTURE(3.0f, STATE_STARTUP);

    update_leds_at(&leds, &state, &motor, FS_BOTH, 3.5f);

    EXPECT_FLOAT_NEAR(leds.last_updated, 3.5f);
    EXPECT_FLOAT_NEAR(leds.on_off_fade, 0.0f);
    EXPECT_FLOAT_NEAR(leds.left_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(leds.right_sensor, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 0);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_disabled_state_paints_disabled_animation_and_resets_fade(void) {
    LEDS_UPDATE_FIXTURE(5.0f, STATE_READY);
    update_leds_at(&leds, &state, &motor, FS_NONE, 5.1f);
    EXPECT_FLOAT_NEAR(leds.on_off_fade, 0.1f);

    state.state = STATE_DISABLED;
    update_leds_at(&leds, &state, &motor, FS_NONE, 5.2f);

    EXPECT_FLOAT_NEAR(leds.on_off_fade, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_FLOAT_NEAR(leds.animation_start, 5.2f);
    EXPECT_FLOAT_NEAR(leds.status_idle_time, 5.2f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_time, 5.2f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_lifted_ready_board_blends_status_onto_front_strip(void) {
    LEDS_UPDATE_FIXTURE(6.0f, STATE_READY);

    vesc_if_fake_set_imu(deg2rad(65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 6.1f);

    EXPECT_TRUE(leds.board_is_upright);
    EXPECT_FLOAT_NEAR(leds.status_on_front_blend, 1.0f);
    EXPECT_FLOAT_NEAR(leds.front_strip.brightness, cfg.front.brightness - 0.1f);
    EXPECT_FLOAT_NEAR(leds.rear_strip.brightness, cfg.rear.brightness - 0.1f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_time, 6.1f);

    vesc_if_fake_set_imu(deg2rad(45.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 6.2f);

    EXPECT_TRUE(!leds.board_is_upright);
    EXPECT_FLOAT_NEAR(leds.status_on_front_blend, 0.9f);
    EXPECT_FLOAT_NEAR(leds.animation_start, 6.2f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_running_state_hides_sensor_indicators_when_configured(void) {
    LEDS_UPDATE_FIXTURE(7.0f, STATE_READY);
    update_leds_at(&leds, &state, &motor, FS_BOTH, 7.1f);
    EXPECT_FLOAT_NEAR(leds.left_sensor, 1.0f / 3.0f);
    EXPECT_FLOAT_NEAR(leds.right_sensor, 1.0f / 3.0f);

    state.state = STATE_RUNNING;
    update_leds_at(&leds, &state, &motor, FS_BOTH, 7.2f);

    EXPECT_FLOAT_NEAR(leds.left_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(leds.right_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(leds.split_distance, 0.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_running_entry_direction_and_headlight_transition(void) {
    LEDS_UPDATE_FIXTURE(15.0f, STATE_READY);

    update_leds_at(&leds, &state, &motor, FS_NONE, 15.1f);
    EXPECT_TRUE(!leds.headlights_on);
    EXPECT_TRUE(leds.direction_forward);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);

    state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-3.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 2.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 15.2f);

    EXPECT_TRUE(!leds.direction_forward);
    EXPECT_FLOAT_NEAR(leds.dir_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(leds.headlights_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(leds.headlights_time, 15.2f);
    EXPECT_TRUE(!leds.headlights_on);
    EXPECT_TRUE(leds.front_time_target == &cfg.taillights);
    EXPECT_TRUE(leds.rear_time_target == &cfg.headlights);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 15.8f);
    EXPECT_TRUE(!leds.headlights_on);
    EXPECT_FLOAT_NEAR(leds.headlights_trans.split, 0.2f);

    vesc_if_fake_set_motor_telemetry(0, 0, 4.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 16.3f);
    EXPECT_TRUE(leds.headlights_on);
    EXPECT_FLOAT_NEAR(leds.headlights_time, 0.0f);
    EXPECT_FLOAT_NEAR(leds.headlights_trans.split, 1.0f);
    EXPECT_FLOAT_NEAR(leds.split_distance, 4.0f);
    EXPECT_TRUE(leds.front_bar == &cfg.taillights);
    EXPECT_TRUE(leds.rear_bar == &cfg.headlights);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_running_transition_repaints_each_tick(void) {
    reset_fakes(20.0f);

    CfgLeds cfg = default_leds_cfg();
    CfgHwLeds hw = default_hw_cfg();
    LEDS_UPDATE_FROM_CONFIG(STATE_READY);

    update_leds_at(&leds, &state, &motor, FS_NONE, 20.1f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);

    state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-4.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 3.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 20.2f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_FLOAT_NEAR(leds.dir_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(leds.headlights_trans.split, -1.0f);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.4f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 20.4f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 3);
    EXPECT_TRUE(leds.headlights_trans.split > -1.0f);
    EXPECT_TRUE(leds.headlights_trans.split < 1.0f);

    float split_after_first_transition_tick = leds.headlights_trans.split;
    vesc_if_fake_set_motor_telemetry(0, 0, 3.8f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 20.6f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 4);
    EXPECT_TRUE(leds.headlights_trans.split > split_after_first_transition_tick);
    EXPECT_TRUE(leds.headlights_trans.split < 1.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_cipher_transition_golden_colors(void) {
    reset_fakes(30.0f);

    CfgLeds cfg = default_leds_cfg();
    cfg.headlights_transition = LED_TRANS_CIPHER;
    cfg.direction_transition = LED_TRANS_CIPHER;
    cfg.front = solid_bar(1.0f, COLOR_BLUE);
    cfg.rear = solid_bar(1.0f, COLOR_GREEN);
    cfg.headlights = solid_bar(1.0f, COLOR_WHITE_RGB);
    cfg.taillights = solid_bar(1.0f, COLOR_RED);
    cfg.status.idle_timeout = 0;

    CfgHwLeds hw = default_hw_cfg();
    LEDS_UPDATE_FROM_CONFIG(STATE_READY);

    update_leds_at(&leds, &state, &motor, FS_NONE, 30.1f);

    state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-4.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 2.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 30.2f);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 30.6f);

    EXPECT_TRUE(leds.front_strip.length == 3);
    EXPECT_TRUE(leds.rear_strip.length == 3);
    EXPECT_TRUE(leds.front_strip.data[0] == 0x00000000u);
    EXPECT_TRUE(leds.front_strip.data[1] == 0x00274d2cu);
    EXPECT_TRUE(leds.front_strip.data[2] == 0x00264c23u);
    EXPECT_TRUE(leds.rear_strip.data[0] == 0x00264c23u);
    EXPECT_TRUE(leds.rear_strip.data[1] == 0x00274d2cu);
    EXPECT_TRUE(leds.rear_strip.data[2] == 0x00000000u);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 3);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_setup_rejects_front_rear_count_over_limit(void) {
    LEDS_DEFAULT_FIXTURE(1.0f);
    hw.front.count = LEDS_FRONT_AND_REAR_COUNT_MAX;
    hw.rear.count = 1;
    LEDS_SETUP_FROM_CONFIG();

    EXPECT_TRUE(leds.led_data == NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 0);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 0);
    EXPECT_TRUE(leds.front_strip.length == LEDS_FRONT_AND_REAR_COUNT_MAX);
    EXPECT_TRUE(leds.rear_strip.length == 1);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_runtime_enabled_override_fades_lights_off(void) {
    LEDS_UPDATE_FIXTURE(8.0f, STATE_READY);
    update_leds_at(&leds, &state, &motor, FS_NONE, 8.1f);
    EXPECT_FLOAT_NEAR(leds.on_off_fade, 0.1f);

    leds_set_enabled(&leds, false);
    update_leds_at(&leds, &state, &motor, FS_NONE, 8.2f);

    EXPECT_FLOAT_NEAR(leds.on_off_fade, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_TRUE(!leds_get_runtime_status(&leds)->enabled);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_headlights_runtime_override_uses_off_status_brightness(void) {
    LEDS_UPDATE_FIXTURE(9.0f, STATE_READY);
    EXPECT_FLOAT_NEAR(leds.status_strip.brightness, cfg.status.brightness_headlights_on);

    leds_set_headlights_enabled(&leds, false);
    update_leds_at(&leds, &state, &motor, FS_NONE, 9.1f);

    EXPECT_TRUE(!leds_get_runtime_status(&leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(leds.status_strip.brightness, cfg.status.brightness_headlights_on - 0.1f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_status_idle_blend_starts_after_timeout_and_resets_on_sensor(void) {
    LEDS_UPDATE_FIXTURE(12.0f, STATE_READY);

    update_leds_at(&leds, &state, &motor, FS_NONE, 12.1f);
    EXPECT_FLOAT_NEAR(leds.status_idle_time, 12.1f);

    update_leds_at(&leds, &state, &motor, FS_NONE, 14.2f);
    EXPECT_FLOAT_NEAR(leds.status_idle_blend, 0.1f);
    EXPECT_FLOAT_NEAR(leds.status_animation_start, 14.2f);

    update_leds_at(&leds, &state, &motor, FS_LEFT, 14.3f);
    EXPECT_FLOAT_NEAR(leds.status_idle_time, 14.3f);
    EXPECT_FLOAT_NEAR(leds.status_idle_blend, 0.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_status_on_front_idle_blend_edges(void) {
    LEDS_UPDATE_FIXTURE(22.0f, STATE_READY);

    vesc_if_fake_set_imu(deg2rad(65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 22.1f);
    EXPECT_TRUE(leds.board_is_upright);
    EXPECT_FLOAT_NEAR(leds.status_on_front_blend, 1.0f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_time, 22.1f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_blend, 0.0f);

    update_leds_at(&leds, &state, &motor, FS_NONE, 25.2f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_blend, 0.1f);

    update_leds_at(&leds, &state, &motor, FS_RIGHT, 25.3f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_time, 25.3f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_blend, 0.0f);

    cfg.lights_off_when_lifted = false;
    update_leds_at(&leds, &state, &motor, FS_NONE, 28.4f);
    EXPECT_FLOAT_NEAR(leds.status_on_front_idle_blend, 0.0f);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_status_battery_bar_edges(void) {
    LEDS_UPDATE_FIXTURE(20.0f, STATE_READY);

    vesc_if_fake_set_battery_level(0.5f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 20.1f);

    EXPECT_TRUE(leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(leds.status_strip.data[1] != 0u);
    EXPECT_TRUE(leds.status_strip.data[0] == leds.status_strip.data[1]);
    EXPECT_TRUE(leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(leds.status_strip.data[3] == 0u);

    uint32_t mid_battery_first_led = leds.status_strip.data[0];
    vesc_if_fake_set_battery_level(0.0f);
    update_leds_at(&leds, &state, &motor, FS_NONE, 20.2f);

    EXPECT_TRUE(leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(leds.status_strip.data[0] != mid_battery_first_led);
    EXPECT_TRUE(leds.status_strip.data[1] == 0u);
    EXPECT_TRUE(leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(leds.status_strip.data[3] == 0u);

    leds_destroy(&leds);
    return true;
}

static bool test_leds_status_utilization_hysteresis_edges(void) {
    reset_fakes(18.0f);
    CfgLeds cfg = default_leds_cfg();
    cfg.status.motor_utilization_threshold = 0.5f;
    CfgHwLeds hw = default_hw_cfg();
    LEDS_UPDATE_FROM_CONFIG(STATE_RUNNING);

    motor.duty_cycle.value = 0.54f;
    update_leds_at(&leds, &state, &motor, FS_NONE, 18.1f);
    EXPECT_FLOAT_NEAR(leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    motor.duty_cycle.value = 0.405f;
    update_leds_at(&leds, &state, &motor, FS_NONE, 18.2f);
    EXPECT_FLOAT_NEAR(leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    motor.duty_cycle.value = 0.35f;
    update_leds_at(&leds, &state, &motor, FS_NONE, 18.3f);
    EXPECT_FLOAT_NEAR(leds.status_utilization_blend, 0.0f);

    motor.duty_cycle.value = 0.0f;
    motor.motor_current_saturation = 0.9f;
    update_leds_at(&leds, &state, &motor, FS_NONE, 18.4f);
    EXPECT_FLOAT_NEAR(leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    motor.motor_current_saturation = 0.0f;
    motor.battery_current_saturation = 0.95f;
    update_leds_at(&leds, &state, &motor, FS_NONE, 18.5f);
    EXPECT_FLOAT_NEAR(leds.status_utilization_blend, 10.0f / LEDS_REFRESH_RATE);

    leds_destroy(&leds);
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
