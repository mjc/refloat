static bool test_leds_animation_modes_render_without_oob_writes(void) {
    static const LedAnimMode modes[] = {
        LED_ANIM_SOLID,
        LED_ANIM_FADE,
        LED_ANIM_PULSE,
        LED_ANIM_STROBE,
        LED_ANIM_KNIGHT_RIDER,
        LED_ANIM_FELONY,
        LED_ANIM_RAINBOW_CYCLE,
        LED_ANIM_RAINBOW_FADE,
        LED_ANIM_RAINBOW_ROLL,
    };
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 40.0f, STATE_READY);
    fixture.cfg.front.color2 = COLOR_RED;
    fixture.cfg.rear.color2 = COLOR_BLUE;
    fixture.cfg.front.speed = 1.0f;
    fixture.cfg.rear.speed = 1.0f;

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        fixture.cfg.front.mode = modes[i];
        fixture.cfg.rear.mode = modes[i];
        float now = 40.1f + (float) i * 0.11f;
        update_leds_at(&fixture, FS_NONE, now);
    }

    fixture.cfg.front.mode = LED_ANIM_STROBE;
    fixture.cfg.rear.mode = LED_ANIM_STROBE;
    update_leds_at(&fixture, FS_NONE, 41.2f);

    fixture.cfg.front.mode = LED_ANIM_KNIGHT_RIDER;
    fixture.cfg.rear.mode = LED_ANIM_KNIGHT_RIDER;
    for (size_t i = 0; i < 24; ++i) {
        update_leds_at(&fixture, FS_NONE, 42.0f + (float) i * 0.1f);
    }
    fixture.leds.animation_start = 50.0f;
    update_leds_at(&fixture, FS_NONE, 50.1f);
    update_leds_at(&fixture, FS_NONE, 50.7f);

    fixture.cfg.front.mode = LED_ANIM_FELONY;
    fixture.cfg.rear.mode = LED_ANIM_FELONY;
    fixture.leds.animation_start = 45.0f;
    update_leds_at(&fixture, FS_NONE, 45.01f);
    update_leds_at(&fixture, FS_NONE, 45.06f);
    update_leds_at(&fixture, FS_NONE, 45.11f);

    EXPECT_TRUE(led_driver_fake_paint_calls() == sizeof(modes) / sizeof(modes[0]) + 30);
    EXPECT_TRUE(fixture.leds.front_strip.data != NULL);
    EXPECT_TRUE(fixture.leds.rear_strip.data != NULL);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_invalid_animation_and_colors_render_black(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 1.0f, STATE_READY);
    fixture.cfg.front.mode = (LedAnimMode) 255;
    fixture.cfg.rear.mode = LED_ANIM_SOLID;
    fixture.cfg.rear.color1 = (LedColor) 255;

    update_leds_at(&fixture, FS_NONE, 1.1f);

    for (uint8_t i = 0; i < fixture.leds.front_strip.length; ++i) {
        EXPECT_EQ_U32(fixture.leds.front_strip.data[i], 0u);
    }
    for (uint8_t i = 0; i < fixture.leds.rear_strip.length; ++i) {
        EXPECT_EQ_U32(fixture.leds.rear_strip.data[i], 0u);
    }
    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_rainbow_cycle_handles_long_uptime(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 0.0f, STATE_READY);
    fixture.cfg.front.mode = LED_ANIM_RAINBOW_CYCLE;
    fixture.cfg.front.speed = 1.0f;

    update_leds_at(&fixture, FS_NONE, 100000.25f);

    EXPECT_TRUE(fixture.leds.front_strip.data[0] != 0u);
    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_animation_time_survives_vesc_tick_wrap(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 429496.5f, STATE_READY);
    fixture.cfg.front.mode = LED_ANIM_RAINBOW_FADE;
    fixture.cfg.front.speed = 1.0f;

    update_leds_at(&fixture, FS_NONE, 0.25f);

    EXPECT_TRUE(fixture.leds.front_strip.data[0] != 0u);
    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_negative_animation_speed_is_stopped(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 0.0f, STATE_READY);
    fixture.cfg.front.mode = LED_ANIM_RAINBOW_FADE;
    fixture.cfg.front.speed = -1.0f;

    for (uint8_t i = 1; i <= 10u; ++i) {
        update_leds_at(&fixture, FS_NONE, i * 0.1f);
    }
    update_leds_at(&fixture, FS_NONE, 2.25f);
    uint32_t color = fixture.leds.front_strip.data[0];
    update_leds_at(&fixture, FS_NONE, 3.75f);

    EXPECT_EQ_U32(fixture.leds.front_strip.data[0], color);
    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_bounds_serialized_animation_and_status_config(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 0.0f, STATE_READY);

    fixture.cfg.status.motor_utilization_threshold = 3.0f;
    leds_configure(&fixture.leds, &fixture.cfg);
    EXPECT_FLOAT_NEAR(fixture.leds.motor_utilization_threshold, 0.95f);

    // All five configurable LedBar instances route through this animator.
    fixture.cfg.front.mode = LED_ANIM_STROBE;
    fixture.cfg.front.color1 = COLOR_RED;
    fixture.cfg.front.color2 = COLOR_BLUE;
    fixture.cfg.front.brightness = 1.0f;
    fixture.cfg.front.speed = 32.767f;
    leds_configure(&fixture.leds, &fixture.cfg);
    update_leds_at(&fixture, FS_NONE, 0.01f);
    update_leds_at(&fixture, FS_NONE, 0.06f);
    EXPECT_TRUE(((fixture.leds.front_strip.data[0] >> 16) & 0xFFu) > 0);
    EXPECT_EQ_U32(fixture.leds.front_strip.data[0] & 0xFFu, 0u);

    fixture.cfg.front.speed = NAN;
    update_leds_at(&fixture, FS_NONE, 0.07f);
    EXPECT_TRUE(((fixture.leds.front_strip.data[0] >> 16) & 0xFFu) > 0);
    EXPECT_EQ_U32(fixture.leds.front_strip.data[0] & 0xFFu, 0u);

    // Every internal brightness setting reaches led_set_color. Values above
    // the XML maximum must not amplify lower color channels or change hue.
    fixture.cfg.front.mode = LED_ANIM_SOLID;
    fixture.cfg.front.color1 = COLOR_FLAME;
    fixture.cfg.front.brightness = 3.0f;
    leds_configure(&fixture.leds, &fixture.cfg);
    for (uint8_t i = 1; i <= 40; ++i) {
        update_leds_at(&fixture, FS_NONE, 0.06f + i * 0.01f);
    }
    EXPECT_EQ_U32(fixture.leds.front_strip.data[0], 0x00FF5000u);

    fixture.leds.front_strip.brightness = NAN;
    update_leds_at(&fixture, FS_NONE, 0.47f);
    EXPECT_EQ_U32(fixture.leds.front_strip.data[0], 0u);
    fixture.leds.front_strip.brightness = 1.0f;

    fixture.cfg.status.red_bar_percentage = 1.0f;
    fixture.cfg.status.brightness_headlights_off = 1.0f;
    fixture.cfg.status.brightness_headlights_on = 1.0f;
    fixture.leds.status_strip.brightness = 1.0f;
    vesc_if_fake_set_battery_level(0.75f);
    update_leds_at(&fixture, FS_NONE, 0.5f);
    EXPECT_EQ_U32(fixture.leds.status_strip.data[0], 0x00909090u);

    fixture.cfg.status.red_bar_percentage = NAN;
    update_leds_at(&fixture, FS_NONE, 0.51f);
    EXPECT_EQ_U32(fixture.leds.status_strip.data[0], 0x00909090u);

    fixture.cfg.status.idle_timeout = UINT16_MAX;
    update_leds_at(&fixture, FS_NONE, 301.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_blend, 0.1f);

    leds_fixture_destroy(&fixture);
    return true;
}
