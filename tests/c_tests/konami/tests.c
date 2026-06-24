static bool test_konami_sequence_and_timeout(void) {
    feedback_fakes_reset();
    Time time = {.now = 2000u};
    const FootpadSensorState sequence[] = {FS_LEFT, FS_RIGHT, FS_BOTH};
    Konami konami;
    konami_init(&konami, sequence, 3);

    FootpadSensor fs = {.state = FS_LEFT};
    Leds leds = {0};

    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 0);

    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);

    time.now += 2000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);

    fs.state = FS_BOTH;
    time.now += 200u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);

    fs.state = FS_LEFT;
    time.now += 2000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);

    time.now += 6000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);

    time.now += 2000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);
    fs.state = FS_RIGHT;
    time.now += 2000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 2);
    fs.state = FS_BOTH;
    time.now += 2000u;
    EXPECT_TRUE(konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 1);

    return true;
}

static bool test_konami_boundary_and_idle_inputs(void) {
    feedback_fakes_reset();
    const FootpadSensorState sequence[] = {FS_LEFT, FS_RIGHT};
    Konami konami;
    konami_init(&konami, sequence, 2);

    Time time = {.now = 10000u};
    FootpadSensor fs = {.state = FS_RIGHT};
    Leds leds = {0};

    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 0);

    fs.state = FS_LEFT;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);
    EXPECT_EQ_U32(konami.timer, time.now);

    time.now += 1500u;
    fs.state = FS_RIGHT;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);

    time.now += 1u;
    EXPECT_TRUE(konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 1);

    fs.state = FS_LEFT;
    time.now += 2000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);

    time.now += 5000u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 1);

    time.now += 1u;
    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 1);

    return true;
}

static bool test_konami_single_step_sequence(void) {
    feedback_fakes_reset();
    const FootpadSensorState sequence[] = {FS_BOTH};
    Konami konami;
    konami_init(&konami, sequence, 1);

    Time time = {.now = 2000u};
    FootpadSensor fs = {.state = FS_LEFT};
    Leds leds = {0};

    EXPECT_TRUE(!konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 0);

    fs.state = FS_BOTH;
    EXPECT_TRUE(konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 1);

    EXPECT_TRUE(konami_check(&konami, &leds, &fs, &time));
    EXPECT_TRUE(konami.state == 0);
    EXPECT_TRUE(feedback_fakes_led_confirm_calls() == 2);

    return true;
}
