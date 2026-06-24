static bool expect_byte_bits(
    const uint16_t *bits, size_t offset, uint8_t value, uint16_t zero, uint16_t one
) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        uint16_t expected = (value & (1u << bit)) != 0 ? one : zero;
        EXPECT_TRUE(bits[offset + bit] == expected);
    }
    return true;
}

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

static bool test_led_driver_setup_and_color_encoding(void) {
    vesc_if_fake_reset();

    uint32_t front_data[] = {0x00FF0000u, 0xFF102080u};
    uint32_t rear_data[] = {0xFF0000FFu};

    LedStrip front = {
        .data = front_data,
        .length = 2,
        .color_order = LED_COLOR_GRB,
    };
    LedStrip rear = {
        .data = rear_data,
        .length = 1,
        .color_order = LED_COLOR_WRGB,
    };
    const LedStrip *strips[STRIP_COUNT] = {&front, &rear, NULL};

    LedDriver driver;
    led_driver_init(&driver);
    EXPECT_TRUE(driver.bitbuffer == NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 0);

    EXPECT_TRUE(led_driver_setup(&driver, LED_PIN_B7, LED_PIN_CFG_PULLUP_TO_5V, strips));
    EXPECT_TRUE(driver.pin == LED_PIN_B7);
    EXPECT_TRUE(driver.bitbuffer != NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 24u * front.length + 32u * rear.length + 1u);
    EXPECT_TRUE(driver.strip_bitbuffs[0] == driver.bitbuffer);
    EXPECT_TRUE(driver.strip_bitbuffs[1] == driver.bitbuffer + 24u * front.length);
    EXPECT_TRUE(driver.strip_bitbuffs[2] == NULL);
    EXPECT_TRUE(vesc_if_fake_set_pad_mode_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_last_pad_gpio() == GPIOB);
    EXPECT_EQ_U32(vesc_if_fake_last_pad_pin(), 7u);
    EXPECT_TRUE((vesc_if_fake_last_pad_mode() & PAL_STM32_OTYPE_OPENDRAIN) != 0);
    EXPECT_TRUE(TIM4->ARR == 104u);
    EXPECT_TRUE((TIM4->DIER & TIM_DMA_CC2) != 0);
    EXPECT_TRUE((DMA1_Stream3->CR & DMA_SxCR_EN) != 0);
    EXPECT_TRUE(DMA1_Stream3->NDTR == driver.bitbuffer_length);
    EXPECT_TRUE(driver.bitbuffer[driver.bitbuffer_length - 1] == 0u);

    led_driver_paint(&driver);

    const uint16_t zero = 31u;
    const uint16_t one = 72u;

    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, 0, 0x00u, zero, one));
    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, 8, 0xFFu, zero, one));
    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, 16, 0x00u, zero, one));

    const size_t rear_offset = 24u * front.length;
    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, rear_offset + 0, 0xFFu, zero, one));
    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, rear_offset + 8, 0x00u, zero, one));
    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, rear_offset + 16, 0x00u, zero, one));
    EXPECT_TRUE(expect_byte_bits(driver.bitbuffer, rear_offset + 24, 0xFFu, zero, one));

    EXPECT_TRUE((TIM4->DIER & TIM_DMA_CC2) != 0);
    EXPECT_TRUE((DMA1_Stream3->CR & DMA_SxCR_EN) != 0);
    EXPECT_TRUE((DMA1->LIFCR & (DMA_LIFCR_CTCIF0 << 22u)) != 0);

    led_driver_destroy(&driver);
    EXPECT_TRUE(driver.bitbuffer == NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 0);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);
    EXPECT_TRUE((DMA1_Stream3->CR & DMA_SxCR_EN) == 0);

    return true;
}

static bool test_led_driver_rejects_invalid_pin(void) {
    vesc_if_fake_reset();

    LedDriver driver;
    led_driver_init(&driver);
    const LedStrip *strips[STRIP_COUNT] = {NULL};

    EXPECT_TRUE(!led_driver_setup(&driver, (LedPin) (LED_PIN_LAST + 1), LED_PIN_CFG_NO_PULLUP, strips));
    EXPECT_TRUE(driver.bitbuffer == NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 0);
    EXPECT_TRUE(vesc_if_fake_set_pad_mode_calls() == 0);

    return true;
}

static bool test_led_driver_rejects_invalid_color_order(void) {
    vesc_if_fake_reset();

    uint32_t data[] = {0x00ffffffu};
    LedStrip strip = {
        .data = data,
        .length = 1,
        .color_order = (LedColorOrder) 99,
    };
    const LedStrip *strips[STRIP_COUNT] = {&strip, NULL};

    LedDriver driver;
    led_driver_init(&driver);

    EXPECT_TRUE(!led_driver_setup(&driver, LED_PIN_B6, LED_PIN_CFG_NO_PULLUP, strips));
    EXPECT_TRUE(driver.bitbuffer == NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 0);
    EXPECT_TRUE(vesc_if_fake_set_pad_mode_calls() == 0);

    led_driver_paint(&driver);
    EXPECT_TRUE(driver.bitbuffer == NULL);

    return true;
}

static bool test_led_driver_rejects_oversized_strip_count(void) {
    vesc_if_fake_reset();

    uint32_t data[1] = {0x00ffffffu};
    LedStrip strip = {
        .data = data,
        .length = 31,
        .color_order = LED_COLOR_GRB,
    };
    const LedStrip *strips[STRIP_COUNT] = {&strip, NULL, NULL};

    LedDriver driver;
    led_driver_init(&driver);

    bool setup_ok = led_driver_setup(&driver, LED_PIN_B6, LED_PIN_CFG_NO_PULLUP, strips);
    if (setup_ok) {
        led_driver_destroy(&driver);
    }

    EXPECT_TRUE(!setup_ok);
    EXPECT_TRUE(driver.bitbuffer == NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 0);
    EXPECT_TRUE(vesc_if_fake_set_pad_mode_calls() == 0);

    return true;
}

static bool test_led_driver_alternate_pins_and_noop_paths(void) {
    vesc_if_fake_reset();

    LedDriver driver;
    led_driver_init(&driver);
    led_driver_paint(&driver);
    led_driver_destroy(&driver);
    EXPECT_TRUE(driver.bitbuffer == NULL);
    EXPECT_TRUE(driver.bitbuffer_length == 0);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 0);

    uint32_t data[] = {0xFFFFFFFFu};
    LedStrip strip = {.data = data, .length = 1, .color_order = LED_COLOR_GRBW};
    const LedStrip *strips[STRIP_COUNT] = {&strip, NULL, NULL};

    EXPECT_TRUE(led_driver_setup(&driver, LED_PIN_B6, LED_PIN_CFG_NO_PULLUP, strips));
    EXPECT_TRUE(driver.pin == LED_PIN_B6);
    EXPECT_TRUE(driver.bitbuffer_length == 33u);
    EXPECT_TRUE(vesc_if_fake_last_pad_gpio() == GPIOB);
    EXPECT_EQ_U32(vesc_if_fake_last_pad_pin(), 6u);
    EXPECT_TRUE((vesc_if_fake_last_pad_mode() & PAL_STM32_OTYPE_OPENDRAIN) == 0);
    EXPECT_TRUE((TIM4->DIER & TIM_DMA_CC1) != 0);
    EXPECT_TRUE((DMA1_Stream0->CR & DMA_SxCR_EN) != 0);
    led_driver_destroy(&driver);
    EXPECT_TRUE((DMA1_Stream0->CR & DMA_SxCR_EN) == 0);

    EXPECT_TRUE(led_driver_setup(&driver, LED_PIN_C9, LED_PIN_CFG_PULLUP_TO_5V, strips));
    EXPECT_TRUE(driver.pin == LED_PIN_C9);
    EXPECT_TRUE(vesc_if_fake_last_pad_gpio() == GPIOC);
    EXPECT_EQ_U32(vesc_if_fake_last_pad_pin(), 9u);
    EXPECT_TRUE((vesc_if_fake_last_pad_mode() & PAL_STM32_OTYPE_OPENDRAIN) != 0);
    EXPECT_TRUE((TIM3->DIER & TIM_DMA_CC4) != 0);
    EXPECT_TRUE((DMA1_Stream2->CR & DMA_SxCR_EN) != 0);
    led_driver_destroy(&driver);
    EXPECT_TRUE((DMA1_Stream2->CR & DMA_SxCR_EN) == 0);

    return true;
}

static bool expect_byte_bits_msb(
    const uint16_t *bits, size_t offset, uint8_t value, uint16_t zero, uint16_t one
) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        uint16_t expected = (value & (1u << (7u - bit))) != 0 ? one : zero;
        EXPECT_TRUE(bits[offset + bit] == expected);
    }
    return true;
}

static bool test_led_driver_full_brightness_color_orders(void) {
    vesc_if_fake_reset();

    const struct {
        LedColorOrder order;
        uint8_t bytes[4];
        uint8_t bit_count;
    } cases[] = {
        {LED_COLOR_GRB, {0x90u, 0xc4u, 0x64u, 0x00u}, 24u},
        {LED_COLOR_RGB, {0xc4u, 0x90u, 0x64u, 0x00u}, 24u},
        {LED_COLOR_GRBW, {0x90u, 0xc4u, 0x64u, 0xffu}, 32u},
        {LED_COLOR_WRGB, {0xffu, 0xc4u, 0x90u, 0x64u}, 32u},
    };

    const uint16_t zero = 31u;
    const uint16_t one = 72u;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint32_t data[] = {0xffe0c0a0u};
        LedStrip strip = {.data = data, .length = 1, .color_order = cases[i].order};
        const LedStrip *strips[STRIP_COUNT] = {&strip, NULL, NULL};

        LedDriver driver;
        led_driver_init(&driver);
        EXPECT_TRUE(led_driver_setup(&driver, LED_PIN_B7, LED_PIN_CFG_PULLUP_TO_5V, strips));
        EXPECT_EQ_U32(driver.bitbuffer_length, cases[i].bit_count + 1u);

        led_driver_paint(&driver);

        for (uint8_t byte = 0; byte < cases[i].bit_count / 8u; ++byte) {
            EXPECT_TRUE(expect_byte_bits_msb(driver.bitbuffer, byte * 8u, cases[i].bytes[byte], zero, one));
        }
        EXPECT_EQ_U32(driver.bitbuffer[driver.bitbuffer_length - 1], 0u);

        led_driver_destroy(&driver);
    }

    EXPECT_EQ_U32(vesc_if_fake_free_calls(), sizeof(cases) / sizeof(cases[0]));

    return true;
}
