#include "footpad_sensor.h"
#include "leds.h"
#include "motor_data.h"
#include "state.h"
#include "vesc_if_fake.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../test_runner.h"

void led_driver_fake_reset(void);
size_t led_driver_fake_setup_calls(void);
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

static void leds_fixture_setup(LedsFixture *fixture, float seconds, RunState initial_state) {
    reset_fakes(seconds);
    fixture->cfg = default_leds_cfg();
    fixture->hw = default_hw_cfg();
    leds_init(&fixture->leds);
    leds_setup(&fixture->leds, &fixture->hw, &fixture->cfg);
    state_init(&fixture->state);
    fixture->state.state = initial_state;
}

static void leds_fixture_destroy(LedsFixture *fixture) {
    leds_destroy(&fixture->leds);
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

int main(void) {
    const TestCase tests[] = {
        TEST_CASE("leds setup configures internal strips and runtime defaults",
                test_leds_setup_configures_internal_strips_and_runtime_defaults),
    };

    RUN_TEST_SUITE("leds summary", tests);
}
