static bool test_led_strip(void) {
    LedStrip strip = {0};
    led_strip_init(&strip);
    EXPECT_TRUE(strip.data == NULL);
    EXPECT_TRUE(strip.length == 0);
    EXPECT_TRUE(strip.color_order == LED_COLOR_GRB);
    EXPECT_TRUE(!strip.reverse);

    CfgLedStrip cfg_strip = {.count = 5, .color_order = LED_COLOR_WRGB, .reverse = true};
    led_strip_configure(&strip, &cfg_strip);
    EXPECT_TRUE(strip.length == 5);
    EXPECT_TRUE(strip.color_order == LED_COLOR_WRGB);
    EXPECT_TRUE(strip.reverse);

    return true;
}
