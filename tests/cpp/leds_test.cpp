#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#define time_t refloat_time_t
extern "C" {
#include "footpad_sensor.h"
#include "leds.h"
#include "motor_data.h"
#include "state.h"
#include "vesc_if_fake.h"

void led_driver_fake_reset(void);
void led_driver_fake_set_setup_result(bool result);
size_t led_driver_fake_setup_calls(void);
size_t led_driver_fake_paint_calls(void);
size_t led_driver_fake_destroy_calls(void);
}
#undef time_t

static void reset_fakes(float seconds) {
    vesc_if_fake_reset();
    led_driver_fake_reset();
    vesc_if_fake_set_seconds(seconds);
    vesc_if_fake_set_imu(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 25.0f, 25.0f);
}

static LedBar solid_bar(float brightness, LedColor color) {
    return LedBar{
        .brightness = brightness,
        .color1 = color,
        .color2 = COLOR_BLACK,
        .mode = LED_ANIM_SOLID,
        .speed = 1.0f,
    };
}

static CfgLeds default_leds_cfg() {
    return CfgLeds{
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
}

static CfgHwLeds default_hw_cfg() {
    return CfgHwLeds{
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
}

static constexpr float kFloatEps = 1e-4f;

#define CHECK_FLOAT_CLOSE(actual, expected) CHECK(std::fabs((actual) - (expected)) < kFloatEps)

struct LedFixture {
    CfgLeds cfg{};
    CfgHwLeds hw{};
    Leds leds{};
    State state{};
    MotorData motor{};
    bool active{false};

    void setup(float seconds) {
        reset_fakes(seconds);
        cfg = default_leds_cfg();
        hw = default_hw_cfg();
        std::memset(&state, 0, sizeof(state));
        std::memset(&motor, 0, sizeof(motor));
        state_init(&state);
        leds_init(&leds);
        leds_setup(&leds, &hw, &cfg);
        active = true;
    }

    void destroy() {
        if (active) {
            leds_destroy(&leds);
            active = false;
        }
    }

    ~LedFixture() {
        destroy();
    }
};

TEST_CASE("leds setup configures internal strips and runtime defaults", "[leds]") {
    LedFixture fixture;
    fixture.setup(4.0f);

    CHECK(fixture.leds.led_data != nullptr);
    CHECK(led_driver_fake_setup_calls() == 1);
    CHECK(vesc_if_fake_malloc_calls() == 1);
    CHECK(fixture.leds.status_strip.data == fixture.leds.led_data);
    CHECK(fixture.leds.front_strip.data == fixture.leds.led_data + fixture.hw.status.count);
    CHECK(
        fixture.leds.rear_strip.data ==
        fixture.leds.led_data + fixture.hw.status.count + fixture.hw.front.count
    );
    CHECK(fixture.leds.status_strip.length == fixture.hw.status.count);
    CHECK(fixture.leds.front_strip.length == fixture.hw.front.count);
    CHECK(fixture.leds.rear_strip.length == fixture.hw.rear.count);
    CHECK_FLOAT_CLOSE(
        fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on
    );
    CHECK_FLOAT_CLOSE(fixture.leds.front_strip.brightness, fixture.cfg.front.brightness);
    CHECK_FLOAT_CLOSE(fixture.leds.rear_strip.brightness, fixture.cfg.rear.brightness);
    CHECK(leds_get_runtime_status(&fixture.leds)->enabled);
    CHECK(leds_get_runtime_status(&fixture.leds)->headlights_enabled);
    CHECK_FLOAT_CLOSE(fixture.leds.status_idle_time, 4.0f);

    fixture.destroy();
    CHECK(fixture.leds.led_data == nullptr);
    CHECK(vesc_if_fake_free_calls() == 1);
    CHECK(led_driver_fake_destroy_calls() == 1);
}

TEST_CASE("leds update paints and fades running lights", "[leds]") {
    LedFixture fixture;
    fixture.setup(1.0f);
    fixture.state.state = STATE_READY;

    vesc_if_fake_set_seconds(1.1f);
    leds_update(&fixture.leds, &fixture.state, &fixture.motor, FS_BOTH);

    CHECK(led_driver_fake_paint_calls() == 1);
    CHECK_FLOAT_CLOSE(fixture.leds.last_updated, 1.1f);
    CHECK_FLOAT_CLOSE(fixture.leds.on_off_fade, 0.1f);
    CHECK_FLOAT_CLOSE(fixture.leds.left_sensor, 1.0f / 3.0f);
    CHECK_FLOAT_CLOSE(fixture.leds.right_sensor, 1.0f / 3.0f);
}

TEST_CASE("leds running entry direction and headlight transition", "[leds]") {
    LedFixture fixture;
    fixture.setup(15.0f);
    fixture.state.state = STATE_READY;

    vesc_if_fake_set_seconds(15.1f);
    leds_update(&fixture.leds, &fixture.state, &fixture.motor, FS_NONE);
    CHECK(!fixture.leds.headlights_on);
    CHECK(fixture.leds.direction_forward);
    CHECK(led_driver_fake_paint_calls() == 1);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(-0.0523599f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0.0f, 0.0f, 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 25.0f, 25.0f);
    vesc_if_fake_set_seconds(15.2f);
    leds_update(&fixture.leds, &fixture.state, &fixture.motor, FS_NONE);

    CHECK(!fixture.leds.direction_forward);
    CHECK_FLOAT_CLOSE(fixture.leds.dir_trans.split, -1.0f);
    CHECK_FLOAT_CLOSE(fixture.leds.headlights_trans.split, -1.0f);
    CHECK_FLOAT_CLOSE(fixture.leds.headlights_time, 15.2f);
    CHECK(!fixture.leds.headlights_on);
    CHECK(fixture.leds.front_time_target == &fixture.cfg.taillights);
    CHECK(fixture.leds.rear_time_target == &fixture.cfg.headlights);

    vesc_if_fake_set_motor_telemetry(0.0f, 0.0f, 3.5f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 25.0f, 25.0f);
    vesc_if_fake_set_seconds(15.8f);
    leds_update(&fixture.leds, &fixture.state, &fixture.motor, FS_NONE);
    CHECK(!fixture.leds.headlights_on);
    CHECK_FLOAT_CLOSE(fixture.leds.headlights_trans.split, 0.2f);

    vesc_if_fake_set_motor_telemetry(0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 25.0f, 25.0f);
    vesc_if_fake_set_seconds(16.3f);
    leds_update(&fixture.leds, &fixture.state, &fixture.motor, FS_NONE);
    CHECK(fixture.leds.headlights_on);
    CHECK_FLOAT_CLOSE(fixture.leds.headlights_time, 0.0f);
    CHECK_FLOAT_CLOSE(fixture.leds.headlights_trans.split, 1.0f);
    CHECK_FLOAT_CLOSE(fixture.leds.split_distance, 4.0f);
    CHECK(fixture.leds.front_bar == &fixture.cfg.taillights);
    CHECK(fixture.leds.rear_bar == &fixture.cfg.headlights);
}
