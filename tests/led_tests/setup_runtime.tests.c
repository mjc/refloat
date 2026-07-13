static bool test_leds_setup_configures_internal_strips_and_runtime_defaults(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 4.0f, STATE_READY);

    EXPECT_TRUE(fixture.leds.led_data != NULL);
    EXPECT_TRUE(led_driver_fake_setup_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_malloc_calls() == 1);
    EXPECT_TRUE(fixture.leds.status_strip.data == fixture.leds.led_data);
    EXPECT_TRUE(fixture.leds.front_strip.data == fixture.leds.led_data + fixture.hw.status.count);
    EXPECT_TRUE(
        fixture.leds.rear_strip.data ==
        fixture.leds.led_data + fixture.hw.status.count + fixture.hw.front.count
    );
    EXPECT_TRUE(fixture.leds.status_strip.length == fixture.hw.status.count);
    EXPECT_TRUE(fixture.leds.front_strip.length == fixture.hw.front.count);
    EXPECT_TRUE(fixture.leds.rear_strip.length == fixture.hw.rear.count);
    EXPECT_FLOAT_NEAR(
        fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on
    );
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
    EXPECT_TRUE(
        fixture.leds.status_strip.data ==
        fixture.leds.led_data + fixture.hw.front.count + fixture.hw.rear.count
    );

    EXPECT_TRUE(fixture.leds.front_strip.length == fixture.hw.front.count);
    EXPECT_TRUE(fixture.leds.rear_strip.length == fixture.hw.rear.count);
    EXPECT_TRUE(fixture.leds.status_strip.length == fixture.hw.status.count);
    EXPECT_TRUE(fixture.leds.front_strip.color_order == LED_COLOR_GRB);
    EXPECT_TRUE(fixture.leds.rear_strip.color_order == LED_COLOR_WRGB);
    EXPECT_TRUE(fixture.leds.status_strip.color_order == LED_COLOR_RGB);
    EXPECT_TRUE(fixture.leds.front_strip.reverse);
    EXPECT_TRUE(!fixture.leds.rear_strip.reverse);
    EXPECT_TRUE(fixture.leds.status_strip.reverse);

    for (uint8_t i = 0;
         i < fixture.hw.front.count + fixture.hw.rear.count + fixture.hw.status.count;
         ++i) {
        EXPECT_TRUE(fixture.leds.led_data[i] == 0u);
    }

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_setup_rejects_strip_count_above_internal_limit(void) {
    for (uint8_t strip = 0; strip < 3u; ++strip) {
        LedsFixture fixture = leds_fixture_prepare(1.0f);
        fixture.hw.mode = LED_MODE_EXTERNAL;
        CfgLedStrip *strips[] = {&fixture.hw.status, &fixture.hw.front, &fixture.hw.rear};
        strips[strip]->count = LED_STRIP_CONFIG_COUNT_MAX + 1;
        leds_fixture_start(&fixture, STATE_READY);

        EXPECT_TRUE(fixture.leds.cfg == &fixture.cfg);
        EXPECT_TRUE(fixture.leds.led_data == NULL);
        EXPECT_EQ_U32(led_driver_fake_setup_calls(), 0u);
        EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 0u);
        leds_fixture_destroy(&fixture);
    }
    return true;
}

static bool test_leds_setup_rejects_duplicate_strip_orders(void) {
    for (uint8_t duplicate = 0; duplicate < 3u; ++duplicate) {
        LedsFixture fixture = leds_fixture_prepare(1.0f);
        fixture.hw.mode = LED_MODE_EXTERNAL;
        if (duplicate == 0u) {
            fixture.hw.front.order = fixture.hw.status.order;
        } else if (duplicate == 1u) {
            fixture.hw.rear.order = fixture.hw.status.order;
        } else {
            fixture.hw.rear.order = fixture.hw.front.order;
        }

        leds_fixture_start(&fixture, STATE_READY);
        EXPECT_TRUE(fixture.leds.cfg == &fixture.cfg);
        EXPECT_TRUE(fixture.leds.led_data == NULL);
        EXPECT_EQ_U32(led_driver_fake_setup_calls(), 0u);
        EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 0u);
        leds_fixture_destroy(&fixture);
    }
    return true;
}

static bool test_leds_setup_rejects_out_of_range_strip_orders(void) {
    for (uint8_t strip = 0; strip < 3u; ++strip) {
        LedsFixture fixture = leds_fixture_prepare(1.0f);
        fixture.hw.mode = LED_MODE_EXTERNAL;
        if (strip == 0u) {
            fixture.hw.status.order = 4;
        } else if (strip == 1u) {
            fixture.hw.front.order = 4;
        } else {
            fixture.hw.rear.order = 4;
        }

        leds_fixture_start(&fixture, STATE_READY);
        EXPECT_TRUE(fixture.leds.cfg == &fixture.cfg);
        EXPECT_TRUE(fixture.leds.led_data == NULL);
        EXPECT_EQ_U32(led_driver_fake_setup_calls(), 0u);
        EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 0u);
        leds_fixture_destroy(&fixture);
    }
    return true;
}

static bool test_leds_preserve_configured_strip_layouts(void) {
    const uint8_t counts[] = {0, 1, LED_STRIP_CONFIG_COUNT_MAX + 1};

    for (uint8_t status_order = 0; status_order <= STRIP_COUNT + 1; ++status_order) {
        for (uint8_t front_order = 0; front_order <= STRIP_COUNT + 1; ++front_order) {
            for (uint8_t rear_order = 0; rear_order <= STRIP_COUNT + 1; ++rear_order) {
                for (size_t status_count = 0; status_count < 3; ++status_count) {
                    for (size_t front_count = 0; front_count < 3; ++front_count) {
                        for (size_t rear_count = 0; rear_count < 3; ++rear_count) {
                            LedsFixture fixture = leds_fixture_prepare(1.0f);
                            fixture.hw.mode = LED_MODE_EXTERNAL;
                            fixture.hw.status.order = status_order;
                            fixture.hw.front.order = front_order;
                            fixture.hw.rear.order = rear_order;
                            fixture.hw.status.count = counts[status_count];
                            fixture.hw.front.count = counts[front_count];
                            fixture.hw.rear.count = counts[rear_count];

                            leds_fixture_start(&fixture, STATE_READY);
                            EXPECT_TRUE(fixture.leds.led_data == NULL);
                            EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 0u);
                            leds_fixture_destroy(&fixture);
                        }
                    }
                }
            }
        }
    }

    return true;
}

static bool test_leds_configure_internal_strip_layout(void) {
    for (uint8_t mask = 0; mask < 8; ++mask) {
        LedsFixture fixture = leds_fixture_prepare(1.0f);
        fixture.cfg.headlights_on = false;
        fixture.hw.status.count = mask & 1 ? 1 : 0;
        fixture.hw.front.count = mask & 2 ? 1 : 0;
        fixture.hw.rear.count = mask & 4 ? 1 : 0;
        leds_fixture_start(&fixture, STATE_READY);

        EXPECT_FLOAT_NEAR(
            fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_off
        );
        EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), mask == 0 ? 0u : 1u);
        EXPECT_TRUE((fixture.leds.led_data != NULL) == (mask != 0));
        leds_fixture_destroy(&fixture);
        EXPECT_EQ_U32(vesc_if_fake_free_calls(), vesc_if_fake_malloc_calls());
    }

    LedsFixture failed = leds_fixture_prepare(1.0f);
    vesc_if_fake_fail_next_malloc();
    leds_fixture_start(&failed, STATE_READY);
    EXPECT_TRUE(failed.leds.led_data == NULL);
    EXPECT_EQ_U32(led_driver_fake_setup_calls(), 0u);
    leds_fixture_destroy(&failed);
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
    EXPECT_FLOAT_NEAR(
        fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on
    );

    fixture.state.state = STATE_RUNNING;
    fixture.leds.headlights_on = true;
    leds_set_headlights_enabled(&fixture.leds, false);
    update_leds_at(&fixture, FS_NONE, 9.1f);

    EXPECT_TRUE(!leds_get_runtime_status(&fixture.leds)->headlights_enabled);
    EXPECT_FLOAT_NEAR(
        fixture.leds.status_strip.brightness, fixture.cfg.status.brightness_headlights_on - 0.1f
    );
    EXPECT_TRUE(fixture.leds.front_time_target == &fixture.cfg.front);
    EXPECT_TRUE(fixture.leds.rear_time_target == &fixture.cfg.rear);

    leds_fixture_destroy(&fixture);
    return true;
}
