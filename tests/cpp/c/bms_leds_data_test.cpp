#include "../c_support.hpp"

namespace {

static bool expect_byte_bits(
    const uint16_t *bits, size_t offset, uint8_t value, uint16_t zero, uint16_t one
) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        uint16_t expected = (value & (1u << bit)) != 0 ? one : zero;
        REQUIRE(bits[offset + bit] == expected);
    }
    return true;
}

static CfgBMS default_bms_cfg(void) {
    return CfgBMS{
        .enabled = true,
        .cell_lv_threshold = 2.5f,
        .cell_hv_threshold = 4.2f,
        .cell_balance_threshold = 0.1f,
        .cell_lt_threshold = -10,
        .cell_ht_threshold = 60,
        .bms_ht_threshold = 70,
    };
}

static void init_data_record(
    DataRecord *dr,
    Sample *storage,
    size_t storage_count,
    bool enabled,
    bool recording,
    bool autostart,
    bool autostop,
    uint8_t decimation,
    uint32_t sample_rate,
    uint32_t sample_count
) {
    *dr = {};
    circular_buffer_init(&dr->buffer, sizeof(Sample), storage_count, storage);
    dr->enabled = enabled;
    dr->recording = recording;
    dr->autostart = autostart;
    dr->autostop = autostop;
    dr->decimation = decimation;
    dr->sample_rate = sample_rate;
    dr->sample_count = sample_count;
}

static const uint8_t *send_data_recorder_request(
    DataRecord *dr, const uint8_t *request, size_t request_len, size_t *len
) {
    data_recorder_request(dr, (uint8_t *) request, request_len);
    return vesc_if_fake_last_app_data(len);
}

#define SEND_DATA_RECORDER_REQUEST(dr_, len_, ...)                                                 \
    send_data_recorder_request(                                                                    \
        (dr_), (const uint8_t[]){__VA_ARGS__}, sizeof((const uint8_t[]){__VA_ARGS__}), &(len_)     \
    )

TEST_CASE("bms faults", "[c]") {
    BMS bms;
    bms_init(&bms);
    CHECK_FLOAT_NEAR(bms.msg_age, 42.0f);
    REQUIRE(bms.fault_mask == BMSF_NONE);

    CfgBMS cfg = default_bms_cfg();

    Time time = {.now = 100000u, .start_timer = 100000u};

    bms.msg_age = 10.0f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(!bms_is_fault(&bms, BMSF_CONNECTION));

    timer_expire(&time, &time.start_timer, 6.0f);
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms_is_fault(&bms, BMSF_CONNECTION));

    bms.msg_age = 0.0f;
    bms.cell_lv = 2.0f;
    bms.cell_hv = 4.3f;
    bms.cell_lt = -20;
    bms.cell_ht = 80;
    bms.bms_ht = 80;
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_UNDER_VOLTAGE));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_OVER_VOLTAGE));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    REQUIRE(bms_is_fault(&bms, BMSF_OVER_TEMP));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_BALANCE));

    cfg.cell_ht_threshold = 0;
    cfg.bms_ht_threshold = 0;
    bms.cell_lt = -20;
    bms.cell_ht = 80;
    bms.bms_ht = 80;
    bms.cell_lv = 3.8f;
    bms.cell_hv = 3.85f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    REQUIRE(!bms_is_fault(&bms, BMSF_OVER_TEMP));
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_BALANCE));

    cfg.enabled = false;
    bms.fault_mask = 0xffffffffu;
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms.fault_mask == BMSF_NONE);
}

TEST_CASE("bms threshold boundaries", "[c]") {
    BMS bms;
    bms_init(&bms);

    CfgBMS cfg = default_bms_cfg();

    Time time = {.now = 50000u, .start_timer = 50000u};
    bms.cell_lv = 3.7f;
    bms.cell_hv = 3.75f;
    bms.cell_lt = 20;
    bms.cell_ht = 25;
    bms.bms_ht = 30;
    bms.msg_age = 5.0f;
    timer_expire(&time, &time.start_timer, 5.0f);
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms.fault_mask == BMSF_NONE);

    bms.msg_age = 5.001f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(!bms_is_fault(&bms, BMSF_CONNECTION));

    time.now += 1u;
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms_is_fault(&bms, BMSF_CONNECTION));

    bms.msg_age = 0.0f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(!bms_is_fault(&bms, BMSF_CONNECTION));

    bms.cell_lv = cfg.cell_lv_threshold;
    bms.cell_hv = cfg.cell_hv_threshold;
    bms.cell_lt = cfg.cell_lt_threshold;
    bms.cell_ht = cfg.cell_ht_threshold;
    bms.bms_ht = cfg.bms_ht_threshold;
    bms.msg_age = 0.0f;
    cfg.cell_balance_threshold = 2.0f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_UNDER_VOLTAGE));
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_OVER_VOLTAGE));
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    REQUIRE(!bms_is_fault(&bms, BMSF_OVER_TEMP));
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_BALANCE));

    cfg.cell_balance_threshold = 0.1f;
    bms.cell_lv = 3.7f;
    bms.cell_hv = 3.8f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(!bms_is_fault(&bms, BMSF_CELL_BALANCE));

    bms.cell_hv += 0.001f;
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_BALANCE));
}

TEST_CASE("bms faults clear on recovery", "[c]") {
    BMS bms;
    bms_init(&bms);

    CfgBMS cfg = default_bms_cfg();

    Time time = {.now = 100000u, .start_timer = 100000u};
    timer_expire(&time, &time.start_timer, 6.0f);

    bms.msg_age = 0.0f;
    bms.cell_lv = 2.0f;
    bms.cell_hv = 4.4f;
    bms.cell_lt = -20;
    bms.cell_ht = 80;
    bms.bms_ht = 90;
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_UNDER_VOLTAGE));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_OVER_VOLTAGE));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    REQUIRE(bms_is_fault(&bms, BMSF_OVER_TEMP));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_BALANCE));

    bms.cell_lv = 3.70f;
    bms.cell_hv = 3.75f;
    bms.cell_lt = 20;
    bms.cell_ht = 25;
    bms.bms_ht = 30;
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms.fault_mask == BMSF_NONE);
}

TEST_CASE("bms startup grace waits for first sample", "[c][red]") {
    BMS bms;
    bms_init(&bms);

    CfgBMS cfg = default_bms_cfg();

    Time time = {.now = 100000u, .start_timer = 100000u};
    bms_update(&bms, &cfg, &time);
    REQUIRE(bms.fault_mask == BMSF_NONE);
}

TEST_CASE("bms is fault none is false", "[c][red]") {
    BMS bms;
    bms_init(&bms);

    bms.fault_mask = BMSF_NONE;
    REQUIRE(!bms_is_fault(&bms, BMSF_NONE));

    bms.fault_mask = 0xffffffffu;
    REQUIRE(!bms_is_fault(&bms, BMSF_NONE));
    REQUIRE(!bms_is_fault(&bms, (BMSFaultCode) 8));
    REQUIRE(!bms_is_fault(&bms, (BMSFaultCode) 99));
    REQUIRE(bms_is_fault(&bms, BMSF_CONNECTION));
    REQUIRE(bms_is_fault(&bms, BMSF_CELL_BALANCE));
}

TEST_CASE("led strip", "[c]") {
    LedStrip strip = {};
    led_strip_init(&strip);
    REQUIRE(strip.data == NULL);
    REQUIRE(strip.length == 0);
    REQUIRE(strip.color_order == LED_COLOR_GRB);
    REQUIRE(!strip.reverse);

    CfgLedStrip cfg_strip = {.count = 5, .color_order = LED_COLOR_WRGB, .reverse = true};
    led_strip_configure(&strip, &cfg_strip);
    REQUIRE(strip.length == 5);
    REQUIRE(strip.color_order == LED_COLOR_WRGB);
    REQUIRE(strip.reverse);
}

TEST_CASE("led driver setup and color encoding", "[c]") {
    vesc_if_fake_reset();

    uint32_t front_data[] = {0x00FF0000u, 0xFF102080u};
    uint32_t rear_data[] = {0xFF0000FFu};
    LedStrip front{};
    front.data = front_data;
    front.length = 2;
    front.color_order = LED_COLOR_GRB;
    LedStrip rear{};
    rear.data = rear_data;
    rear.length = 1;
    rear.color_order = LED_COLOR_WRGB;
    const LedStrip *strips[STRIP_COUNT] = {&front, &rear, NULL};

    LedDriver driver;
    led_driver_init(&driver);
    REQUIRE(driver.bitbuffer == NULL);
    REQUIRE(driver.bitbuffer_length == 0);

    REQUIRE(led_driver_setup(&driver, LED_PIN_B7, LED_PIN_CFG_PULLUP_TO_5V, strips));
    REQUIRE(driver.pin == LED_PIN_B7);
    REQUIRE(driver.bitbuffer != NULL);
    REQUIRE(driver.bitbuffer_length == 24u * front.length + 32u * rear.length + 1u);
    REQUIRE(driver.strip_bitbuffs[0] == driver.bitbuffer);
    REQUIRE(driver.strip_bitbuffs[1] == driver.bitbuffer + 24u * front.length);
    REQUIRE(driver.strip_bitbuffs[2] == NULL);
    REQUIRE(vesc_if_fake_set_pad_mode_calls() == 1);
    REQUIRE(vesc_if_fake_last_pad_gpio() == GPIOB);
    CHECK_U32(vesc_if_fake_last_pad_pin(), 7u);
    REQUIRE((vesc_if_fake_last_pad_mode() & PAL_STM32_OTYPE_OPENDRAIN) != 0);
    REQUIRE(TIM4->ARR == 104u);
    REQUIRE((TIM4->DIER & TIM_DMA_CC2) != 0);
    REQUIRE((DMA1_Stream3->CR & DMA_SxCR_EN) != 0);
    REQUIRE(DMA1_Stream3->NDTR == driver.bitbuffer_length);
    REQUIRE(driver.bitbuffer[driver.bitbuffer_length - 1] == 0u);

    led_driver_paint(&driver);

    const uint16_t zero = 31u;
    const uint16_t one = 72u;

    REQUIRE(expect_byte_bits(driver.bitbuffer, 0, 0x00u, zero, one));
    REQUIRE(expect_byte_bits(driver.bitbuffer, 8, 0xFFu, zero, one));
    REQUIRE(expect_byte_bits(driver.bitbuffer, 16, 0x00u, zero, one));

    const size_t rear_offset = 24u * front.length;
    REQUIRE(expect_byte_bits(driver.bitbuffer, rear_offset + 0, 0xFFu, zero, one));
    REQUIRE(expect_byte_bits(driver.bitbuffer, rear_offset + 8, 0x00u, zero, one));
    REQUIRE(expect_byte_bits(driver.bitbuffer, rear_offset + 16, 0x00u, zero, one));
    REQUIRE(expect_byte_bits(driver.bitbuffer, rear_offset + 24, 0xFFu, zero, one));

    REQUIRE((TIM4->DIER & TIM_DMA_CC2) != 0);
    REQUIRE((DMA1_Stream3->CR & DMA_SxCR_EN) != 0);
    REQUIRE((DMA1->LIFCR & (DMA_LIFCR_CTCIF0 << 22u)) != 0);

    led_driver_destroy(&driver);
    REQUIRE(driver.bitbuffer == NULL);
    REQUIRE(driver.bitbuffer_length == 0);
    REQUIRE(vesc_if_fake_free_calls() == 1);
    REQUIRE((DMA1_Stream3->CR & DMA_SxCR_EN) == 0);
}

TEST_CASE("led driver rejects invalid pin", "[c]") {
    vesc_if_fake_reset();

    LedDriver driver;
    led_driver_init(&driver);
    const LedStrip *strips[STRIP_COUNT] = {NULL};

    REQUIRE(!led_driver_setup(&driver, (LedPin) (LED_PIN_LAST + 1), LED_PIN_CFG_NO_PULLUP, strips));
    REQUIRE(driver.bitbuffer == NULL);
    REQUIRE(driver.bitbuffer_length == 0);
    REQUIRE(vesc_if_fake_set_pad_mode_calls() == 0);
}

TEST_CASE("led driver rejects invalid color order", "[c][red]") {
    vesc_if_fake_reset();

    uint32_t data[] = {0x00ffffffu};
    LedStrip strip{};
    strip.data = data;
    strip.length = 1;
    strip.color_order = (LedColorOrder) 99;
    const LedStrip *strips[STRIP_COUNT] = {&strip, NULL};

    LedDriver driver;
    led_driver_init(&driver);

    REQUIRE(!led_driver_setup(&driver, LED_PIN_B6, LED_PIN_CFG_NO_PULLUP, strips));
    REQUIRE(driver.bitbuffer == NULL);
    REQUIRE(driver.bitbuffer_length == 0);
    REQUIRE(vesc_if_fake_set_pad_mode_calls() == 0);

    led_driver_paint(&driver);
    REQUIRE(driver.bitbuffer == NULL);
}

TEST_CASE("led driver rejects oversized strip count", "[c][red]") {
    vesc_if_fake_reset();

    uint32_t data[1] = {0x00ffffffu};
    LedStrip strip{};
    strip.data = data;
    strip.length = 31;
    strip.color_order = LED_COLOR_GRB;
    const LedStrip *strips[STRIP_COUNT] = {&strip, NULL, NULL};

    LedDriver driver;
    led_driver_init(&driver);

    bool setup_ok = led_driver_setup(&driver, LED_PIN_B6, LED_PIN_CFG_NO_PULLUP, strips);
    if (setup_ok) {
        led_driver_destroy(&driver);
    }

    REQUIRE(!setup_ok);
    REQUIRE(driver.bitbuffer == NULL);
    REQUIRE(driver.bitbuffer_length == 0);
    REQUIRE(vesc_if_fake_set_pad_mode_calls() == 0);
}

TEST_CASE("led driver alternate pins and noop paths", "[c]") {
    vesc_if_fake_reset();

    LedDriver driver;
    led_driver_init(&driver);
    led_driver_paint(&driver);
    led_driver_destroy(&driver);
    REQUIRE(driver.bitbuffer == NULL);
    REQUIRE(driver.bitbuffer_length == 0);
    REQUIRE(vesc_if_fake_free_calls() == 0);

    uint32_t data[] = {0xFFFFFFFFu};
    LedStrip strip = {.data = data, .length = 1, .color_order = LED_COLOR_GRBW};
    const LedStrip *strips[STRIP_COUNT] = {&strip, NULL, NULL};

    REQUIRE(led_driver_setup(&driver, LED_PIN_B6, LED_PIN_CFG_NO_PULLUP, strips));
    REQUIRE(driver.pin == LED_PIN_B6);
    REQUIRE(driver.bitbuffer_length == 33u);
    REQUIRE(vesc_if_fake_last_pad_gpio() == GPIOB);
    CHECK_U32(vesc_if_fake_last_pad_pin(), 6u);
    REQUIRE((vesc_if_fake_last_pad_mode() & PAL_STM32_OTYPE_OPENDRAIN) == 0);
    REQUIRE((TIM4->DIER & TIM_DMA_CC1) != 0);
    REQUIRE((DMA1_Stream0->CR & DMA_SxCR_EN) != 0);
    led_driver_destroy(&driver);
    REQUIRE((DMA1_Stream0->CR & DMA_SxCR_EN) == 0);

    REQUIRE(led_driver_setup(&driver, LED_PIN_C9, LED_PIN_CFG_PULLUP_TO_5V, strips));
    REQUIRE(driver.pin == LED_PIN_C9);
    REQUIRE(vesc_if_fake_last_pad_gpio() == GPIOC);
    CHECK_U32(vesc_if_fake_last_pad_pin(), 9u);
    REQUIRE((vesc_if_fake_last_pad_mode() & PAL_STM32_OTYPE_OPENDRAIN) != 0);
    REQUIRE((TIM3->DIER & TIM_DMA_CC4) != 0);
    REQUIRE((DMA1_Stream2->CR & DMA_SxCR_EN) != 0);
    led_driver_destroy(&driver);
    REQUIRE((DMA1_Stream2->CR & DMA_SxCR_EN) == 0);
}

static bool expect_byte_bits_msb(
    const uint16_t *bits, size_t offset, uint8_t value, uint16_t zero, uint16_t one
) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        uint16_t expected = (value & (1u << (7u - bit))) != 0 ? one : zero;
        REQUIRE(bits[offset + bit] == expected);
    }
    return true;
}

TEST_CASE("led driver full brightness color orders", "[c]") {
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
        REQUIRE(led_driver_setup(&driver, LED_PIN_B7, LED_PIN_CFG_PULLUP_TO_5V, strips));
        CHECK_U32(driver.bitbuffer_length, cases[i].bit_count + 1u);

        led_driver_paint(&driver);

        for (uint8_t byte = 0; byte < cases[i].bit_count / 8u; ++byte) {
            REQUIRE(
                expect_byte_bits_msb(driver.bitbuffer, byte * 8u, cases[i].bytes[byte], zero, one)
            );
        }
        CHECK_U32(driver.bitbuffer[driver.bitbuffer_length - 1], 0u);

        led_driver_destroy(&driver);
    }

    CHECK_U32(vesc_if_fake_free_calls(), sizeof(cases) / sizeof(cases[0]));
}

TEST_CASE("data recorder requests", "[c]") {
    vesc_if_fake_reset();

    Sample storage[2] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 2, true, false, true, true, 1, 100, 2);
    REQUIRE(data_recorder_has_capability(&dr));

    size_t len = 0;
    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 0);
    REQUIRE(payload != NULL);
    REQUIRE(len >= 7);
    CHECK_U32(payload[0], 101u);
    CHECK_U32(payload[1], 41u);
    REQUIRE((payload[3] & 0x2) != 0);
    REQUIRE((payload[3] & 0x1) == 0);

    data_recorder_trigger(&dr, true);
    REQUIRE(dr.recording);
    data_recorder_trigger(&dr, false);
    REQUIRE(!dr.recording);
    dr.autostop = false;
    data_recorder_trigger(&dr, true);
    REQUIRE(dr.recording);

    Data data = {};
    data.state.state = STATE_RUNNING;
    data.state.sat = SAT_CENTERING;
    data.footpad.state = FS_RIGHT;
    data.state.wheelslip = true;
    data.motor.speed = 12.0f;
    data_recorder_sample(&dr, &data, 1234u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 1);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 1);
    REQUIRE(payload != NULL);
    REQUIRE(len > 6);
    CHECK_U32(payload[1], 42u);
    CHECK_U32(payload[2], 0u);
    CHECK_U32(payload[3], 0u);
    CHECK_U32(payload[4], 0u);
    CHECK_U32(payload[5], 1u);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    REQUIRE(payload != NULL);
    REQUIRE(len > 10);
    CHECK_U32(payload[1], 43u);
    CHECK_U32(payload[2], 0u);

    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 1, 1);
    REQUIRE(dr.recording);
    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 1, 0);
    REQUIRE(!dr.recording);

    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 2, 0);
    REQUIRE(!dr.autostart);
    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 3, 1);
    REQUIRE(dr.autostop);
    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 4, 0);
    REQUIRE(dr.decimation == 1);

    dr.recording = true;
    dr.decimation = 1;
    data_recorder_sample(&dr, &data, 2345u);
    data_recorder_sample(&dr, &data, 3456u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 2);
    data_recorder_sample(&dr, &data, 4567u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 2);
}

TEST_CASE("data recorder experiment plot export", "[c][red]") {
    vesc_if_fake_reset();

    Sample storage[3] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 3, true, true, false, false, 1, 100, 3);

    Data data = {};
    data.state.state = STATE_RUNNING;
    data.motor.speed = 1.25f;
    data.motor.duty_cycle.value = 0.5f;
    data.motor.current = 3.0f;

    data_recorder_sample(&dr, &data, 111u);
    data.motor.speed = 2.5f;
    data.motor.current = 6.0f;
    data_recorder_sample(&dr, &data, 222u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 2);

    data_recorder_send_experiment_plot(&dr);

    REQUIRE(vesc_if_fake_plot_init_calls() == 1);
    REQUIRE(vesc_if_fake_plot_add_graph_calls() == ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS));
    REQUIRE(
        vesc_if_fake_plot_set_graph_calls() ==
        circular_buffer_size(&dr.buffer) * ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS)
    );
    REQUIRE(vesc_if_fake_plot_send_points_calls() == vesc_if_fake_plot_set_graph_calls());
    REQUIRE(vesc_if_fake_last_plot_graph() == (int) ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS) - 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_plot_x(), 222.0f);
    REQUIRE(vesc_if_fake_last_plot_y() != 0.0f);
}

TEST_CASE("data recorder request edges", "[c]") {
    vesc_if_fake_reset();

    Sample storage[1] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 1, false, false, true, true, 1, 50, 1);

    size_t len = 123u;
    REQUIRE(SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 0) == NULL);
    CHECK_U32(len, 0u);

    dr.enabled = true;
    REQUIRE(SEND_DATA_RECORDER_REQUEST(&dr, len, 1) == NULL);
    CHECK_U32(len, 0u);

    REQUIRE(SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 4) == NULL);
    CHECK_U32(len, 0u);

    REQUIRE(SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0) == NULL);
    CHECK_U32(len, 0u);

    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 99, 1);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[1], 41u);
    REQUIRE(dr.autostart);
    REQUIRE(dr.autostop);
    REQUIRE(!dr.recording);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[1], 41u);

    Data data = {};
    data.state.state = STATE_RUNNING;
    dr.recording = true;
    dr.decimation = 1;
    data_recorder_sample(&dr, &data, 100u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 1);

    const uint8_t *after_past_end = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 1);
    REQUIRE(after_past_end != NULL);
    CHECK_U32(after_past_end[1], 43u);
    CHECK_U32(after_past_end[2], 0u);
    CHECK_U32(after_past_end[3], 0u);
    CHECK_U32(after_past_end[4], 0u);
    CHECK_U32(after_past_end[5], 1u);
    REQUIRE(len == 6);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[1], 43u);
    CHECK_U32(payload[2], 0u);
    CHECK_U32(payload[3], 0u);
    CHECK_U32(payload[4], 0u);
    CHECK_U32(payload[5], 0u);
}

TEST_CASE("data recorder decimation and sample flags", "[c]") {
    vesc_if_fake_reset();

    Sample storage[2] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 2, true, true, true, true, 3, 100, 2);

    Data data = {};
    data.state.state = STATE_RUNNING;
    data.state.sat = SAT_PB_DUTY;
    data.state.wheelslip = true;
    data.footpad.state = FS_RIGHT;
    data.motor.speed = 12.5f;

    data_recorder_sample(&dr, &data, 100u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 0);
    CHECK_U32(dr.decimation_counter, 1u);
    data_recorder_sample(&dr, &data, 200u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 0);
    CHECK_U32(dr.decimation_counter, 2u);
    data_recorder_sample(&dr, &data, 300u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 1);
    CHECK_U32(dr.decimation_counter, 0u);

    Sample sample = {};
    REQUIRE(circular_buffer_get(&dr.buffer, 0, &sample));
    CHECK_U32(sample.time, 300u);
    CHECK_U32(sample.flags, (SAT_PB_DUTY << 4) | (FS_RIGHT << 2) | 0x2u | 0x1u);

    dr.recording = false;
    data_recorder_sample(&dr, &data, 400u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 1);

    dr.recording = true;
    dr.enabled = false;
    data_recorder_sample(&dr, &data, 500u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 1);
}

TEST_CASE("data recorder sample rate recomputes decimation", "[c][red]") {
    Sample storage[100] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 100, true, false, false, false, 10, 100, 100);

    data_recorder_set_sample_rate(&dr, 200);
    CHECK_U32(dr.sample_rate, 200u);
    CHECK_U32(dr.decimation, 20u);

    data_recorder_set_sample_rate(&dr, 5);
    CHECK_U32(dr.sample_rate, 5u);
    CHECK_U32(dr.decimation, 1u);
}

static uint16_t read_be16(const uint8_t *buf) {
    return ((uint16_t) buf[0] << 8) | buf[1];
}

static uint32_t read_be32(const uint8_t *buf) {
    return ((uint32_t) buf[0] << 24) | ((uint32_t) buf[1] << 16) | ((uint32_t) buf[2] << 8) |
        buf[3];
}

TEST_CASE("data recorder status and data serialization", "[c]") {
    vesc_if_fake_reset();

    Sample storage[3] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 3, true, true, true, false, 7, 1, 1000);

    size_t len = 0;
    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 0);
    REQUIRE(payload != NULL);
    CHECK_U32(len, 7u);
    CHECK_U32(payload[0], 101u);
    CHECK_U32(payload[1], 41u);
    CHECK_U32(payload[2], 1u);
    CHECK_U32(payload[3], 0x03u);
    CHECK_U32(payload[4], 7u);
    CHECK_U32(read_be16(&payload[5]), 65535u);

    Data data = {};
    data.state.state = STATE_RUNNING;
    data.state.sat = SAT_PB_DUTY;
    data.state.wheelslip = true;
    data.footpad.state = FS_BOTH;
    data.imu_freq_tracker.dt = 0.002f;
    data.imu_freq_tracker.frequency.value = 500.0f;
    data.motor.erpm = 1234.0f;
    data.motor.dir_current = -5.5f;
    data.motor.duty_cycle.value = 0.42f;
    data.motor.batt_voltage = 84.0f;
    data.imu.pitch = 1.25f;
    data.imu.balance_pitch = 1.5f;
    data.setpoint = 2.0f;
    data.atr.setpoint.value = 3.0f;
    data.torque_tilt.setpoint.value = 4.0f;
    data.balance_current.value = 5.0f;
    data.atr.transition_boost = 6.0f;

    dr.decimation = 1;
    data_recorder_sample(&dr, &data, 100u);
    data.motor.erpm = 4321.0f;
    data_recorder_sample(&dr, &data, 200u);
    CHECK_U32(circular_buffer_size(&dr.buffer), 2u);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 1);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[0], 101u);
    CHECK_U32(payload[1], 43u);
    CHECK_U32(read_be32(&payload[2]), 1u);
    CHECK_U32(read_be32(&payload[6]), 200u);
    CHECK_U32(payload[10], (SAT_PB_DUTY << 4) | (FS_BOTH << 2) | 0x2u | 0x1u);
    CHECK_U32(len, 11u + 2u * ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS));

    uint16_t second_erpm = 0;
    Sample sample = {};
    REQUIRE(circular_buffer_get(&dr.buffer, 1, &sample));
    second_erpm = sample.values[2];
    CHECK_U32(read_be16(&payload[11 + 2 * 2]), second_erpm);
}

static sigjmp_buf data_recorder_tiny_buffer_sigfpe_env;

enum {
    DATA_RECORDER_TINY_BUFFER_SIGFPE = 8
};

static void catch_data_recorder_tiny_buffer_sigfpe(int signal_number) {
    unused(signal_number);
    siglongjmp(data_recorder_tiny_buffer_sigfpe_env, 1);
}

TEST_CASE("data recorder rejects tiny backing buffer", "[c][red]") {
    vesc_if_fake_reset();

    DataRecord dr = {};
    uint8_t tiny_storage[sizeof(Sample) - 1] = {};
    vesc_if_fake_set_data_buffer(0xcafe1011u, tiny_storage, sizeof(tiny_storage));

    SignalHandler previous_handler =
        signal(DATA_RECORDER_TINY_BUFFER_SIGFPE, catch_data_recorder_tiny_buffer_sigfpe);
    if (sigsetjmp(data_recorder_tiny_buffer_sigfpe_env, 1) != 0) {
        signal(DATA_RECORDER_TINY_BUFFER_SIGFPE, previous_handler);
        FAIL("unexpected signal while exercising C code");
    }

    data_recorder_init(&dr, 100u);

    signal(DATA_RECORDER_TINY_BUFFER_SIGFPE, previous_handler);
    REQUIRE(!data_recorder_has_capability(&dr));
    REQUIRE(!dr.recording);
    REQUIRE(dr.sample_count == 0);
}

TEST_CASE("data recorder data send pauses recording", "[c][red]") {
    vesc_if_fake_reset();

    Sample storage[2] = {};
    DataRecord dr;
    init_data_record(&dr, storage, 2, true, true, true, true, 1, 100, 2);

    Data data = {};
    data.state.state = STATE_RUNNING;
    data_recorder_sample(&dr, &data, 100u);
    REQUIRE(circular_buffer_size(&dr.buffer) == 1);

    size_t len = 0;
    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[1], 43u);
    REQUIRE(!dr.recording);
}

}  // namespace
