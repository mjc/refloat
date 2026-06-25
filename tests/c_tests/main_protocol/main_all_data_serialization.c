enum {
    MAIN_COMMAND_GET_ALLDATA = 10,
};

static bool test_main_all_data_saturates_oversized_float16_fields(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->balance_current.value = 4000.0f;

    uint8_t request[] = {101, MAIN_COMMAND_GET_ALLDATA, 2};
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], MAIN_COMMAND_GET_ALLDATA);
    EXPECT_EQ_U32(payload[2], 2u);

    int32_t index = 3;
    EXPECT_EQ_U32(buffer_get_uint16(payload, &index), 0x7fffu);

    main_protocol_fixture_stop(&fixture);
    return true;
}
