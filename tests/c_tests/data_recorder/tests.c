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
    *dr = (DataRecord) {0};
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

#define SEND_DATA_RECORDER_REQUEST(dr_, len_, ...) \
    send_data_recorder_request( \
        (dr_), \
        (const uint8_t[]) {__VA_ARGS__}, \
        sizeof((const uint8_t[]) {__VA_ARGS__}), \
        &(len_) \
    )

static bool test_data_recorder_requests(void) {
    vesc_if_fake_reset();

    Sample storage[2] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 2, true, false, true, true, 1, 100, 2);
    EXPECT_TRUE(data_recorder_has_capability(&dr));

    size_t len = 0;
    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 0);
    EXPECT_TRUE(payload != NULL);
    EXPECT_TRUE(len >= 7);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], 41u);
    EXPECT_TRUE((payload[3] & 0x2) != 0);
    EXPECT_TRUE((payload[3] & 0x1) == 0);

    data_recorder_trigger(&dr, true);
    EXPECT_TRUE(dr.recording);
    data_recorder_trigger(&dr, false);
    EXPECT_TRUE(!dr.recording);
    dr.autostop = false;
    data_recorder_trigger(&dr, true);
    EXPECT_TRUE(dr.recording);

    Data data = {0};
    data.state.state = STATE_RUNNING;
    data.state.sat = 1;
    data.footpad.state = 2;
    data.state.wheelslip = true;
    data.motor.speed = 12.0f;
    data_recorder_sample(&dr, &data, 1234u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 1);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 1);
    EXPECT_TRUE(payload != NULL);
    EXPECT_TRUE(len > 6);
    EXPECT_EQ_U32(payload[1], 42u);
    EXPECT_EQ_U32(payload[2], 0u);
    EXPECT_EQ_U32(payload[3], 0u);
    EXPECT_EQ_U32(payload[4], 0u);
    EXPECT_EQ_U32(payload[5], 1u);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    EXPECT_TRUE(payload != NULL);
    EXPECT_TRUE(len > 10);
    EXPECT_EQ_U32(payload[1], 43u);
    EXPECT_EQ_U32(payload[2], 0u);

    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 1, 1);
    EXPECT_TRUE(dr.recording);
    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 1, 0);
    EXPECT_TRUE(!dr.recording);

    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 2, 0);
    EXPECT_TRUE(!dr.autostart);
    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 3, 1);
    EXPECT_TRUE(dr.autostop);
    SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 4, 0);
    EXPECT_TRUE(dr.decimation == 1);

    dr.recording = true;
    dr.decimation = 1;
    data_recorder_sample(&dr, &data, 2345u);
    data_recorder_sample(&dr, &data, 3456u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 2);
    data_recorder_sample(&dr, &data, 4567u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 2);

    return true;
}

static bool test_data_recorder_experiment_plot_export(void) {
    vesc_if_fake_reset();

    Sample storage[3] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 3, true, true, false, false, 1, 100, 3);

    Data data = {0};
    data.state.state = STATE_RUNNING;
    data.motor.speed = 1.25f;
    data.motor.duty_cycle.value = 0.5f;
    data.motor.current = 3.0f;

    data_recorder_sample(&dr, &data, 111u);
    data.motor.speed = 2.5f;
    data.motor.current = 6.0f;
    data_recorder_sample(&dr, &data, 222u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 2);

    data_recorder_send_experiment_plot(&dr);

    EXPECT_TRUE(vesc_if_fake_plot_init_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_plot_add_graph_calls() == ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS));
    EXPECT_TRUE(
        vesc_if_fake_plot_set_graph_calls() ==
        circular_buffer_size(&dr.buffer) * ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS)
    );
    EXPECT_TRUE(vesc_if_fake_plot_send_points_calls() == vesc_if_fake_plot_set_graph_calls());
    EXPECT_TRUE(vesc_if_fake_last_plot_graph() == (int) ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS) - 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_plot_x(), 222.0f);
    EXPECT_TRUE(vesc_if_fake_last_plot_y() != 0.0f);

    return true;
}

static bool test_data_recorder_request_edges(void) {
    vesc_if_fake_reset();

    Sample storage[1] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 1, false, false, true, true, 1, 50, 1);

    size_t len = 123u;
    EXPECT_TRUE(SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 0) == NULL);
    EXPECT_EQ_U32(len, 0u);

    dr.enabled = true;
    EXPECT_TRUE(SEND_DATA_RECORDER_REQUEST(&dr, len, 1) == NULL);
    EXPECT_EQ_U32(len, 0u);

    EXPECT_TRUE(SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 4) == NULL);
    EXPECT_EQ_U32(len, 0u);

    EXPECT_TRUE(SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0) == NULL);
    EXPECT_EQ_U32(len, 0u);

    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 99, 1);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[1], 41u);
    EXPECT_TRUE(dr.autostart);
    EXPECT_TRUE(dr.autostop);
    EXPECT_TRUE(!dr.recording);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[1], 41u);

    Data data = {0};
    data.state.state = STATE_RUNNING;
    dr.recording = true;
    dr.decimation = 1;
    data_recorder_sample(&dr, &data, 100u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 1);

    const uint8_t *after_past_end = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 1);
    EXPECT_TRUE(after_past_end != NULL);
    EXPECT_EQ_U32(after_past_end[1], 43u);
    EXPECT_EQ_U32(after_past_end[2], 0u);
    EXPECT_EQ_U32(after_past_end[3], 0u);
    EXPECT_EQ_U32(after_past_end[4], 0u);
    EXPECT_EQ_U32(after_past_end[5], 1u);
    EXPECT_TRUE(len == 6);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[1], 43u);
    EXPECT_EQ_U32(payload[2], 0u);
    EXPECT_EQ_U32(payload[3], 0u);
    EXPECT_EQ_U32(payload[4], 0u);
    EXPECT_EQ_U32(payload[5], 0u);

    return true;
}

static bool test_data_recorder_decimation_and_sample_flags(void) {
    vesc_if_fake_reset();

    Sample storage[2] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 2, true, true, true, true, 3, 100, 2);

    Data data = {0};
    data.state.state = STATE_RUNNING;
    data.state.sat = SAT_PB_DUTY;
    data.state.wheelslip = true;
    data.footpad.state = FS_RIGHT;
    data.motor.speed = 12.5f;

    data_recorder_sample(&dr, &data, 100u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 0);
    EXPECT_EQ_U32(dr.decimation_counter, 1u);
    data_recorder_sample(&dr, &data, 200u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 0);
    EXPECT_EQ_U32(dr.decimation_counter, 2u);
    data_recorder_sample(&dr, &data, 300u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 1);
    EXPECT_EQ_U32(dr.decimation_counter, 0u);

    Sample sample = {0};
    EXPECT_TRUE(circular_buffer_get(&dr.buffer, 0, &sample));
    EXPECT_EQ_U32(sample.time, 300u);
    EXPECT_EQ_U32(sample.flags, (SAT_PB_DUTY << 4) | (FS_RIGHT << 2) | 0x2u | 0x1u);

    dr.recording = false;
    data_recorder_sample(&dr, &data, 400u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 1);

    dr.recording = true;
    dr.enabled = false;
    data_recorder_sample(&dr, &data, 500u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 1);

    return true;
}

static bool test_data_recorder_sample_rate_recomputes_decimation(void) {
    Sample storage[100] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 100, true, false, false, false, 10, 100, 100);

    data_recorder_set_sample_rate(&dr, 200);
    EXPECT_EQ_U32(dr.sample_rate, 200u);
    EXPECT_EQ_U32(dr.decimation, 20u);

    data_recorder_set_sample_rate(&dr, 5);
    EXPECT_EQ_U32(dr.sample_rate, 5u);
    EXPECT_EQ_U32(dr.decimation, 1u);

    return true;
}

static uint16_t read_be16(const uint8_t *buf) {
    return ((uint16_t) buf[0] << 8) | buf[1];
}

static uint32_t read_be32(const uint8_t *buf) {
    return ((uint32_t) buf[0] << 24) | ((uint32_t) buf[1] << 16) | ((uint32_t) buf[2] << 8) | buf[3];
}

static bool test_data_recorder_status_and_data_serialization(void) {
    vesc_if_fake_reset();

    Sample storage[3] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 3, true, true, true, false, 7, 1, 1000);

    size_t len = 0;
    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 1, 0);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 7u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], 41u);
    EXPECT_EQ_U32(payload[2], 1u);
    EXPECT_EQ_U32(payload[3], 0x03u);
    EXPECT_EQ_U32(payload[4], 7u);
    EXPECT_EQ_U32(read_be16(&payload[5]), 65535u);

    Data data = {0};
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
    EXPECT_EQ_U32(circular_buffer_size(&dr.buffer), 2u);

    payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 1);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], 43u);
    EXPECT_EQ_U32(read_be32(&payload[2]), 1u);
    EXPECT_EQ_U32(read_be32(&payload[6]), 200u);
    EXPECT_EQ_U32(payload[10], (SAT_PB_DUTY << 4) | (FS_BOTH << 2) | 0x2u | 0x1u);
    EXPECT_EQ_U32(len, 11u + 2u * ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS));

    uint16_t second_erpm = 0;
    Sample sample = {0};
    EXPECT_TRUE(circular_buffer_get(&dr.buffer, 1, &sample));
    second_erpm = sample.values[2];
    EXPECT_EQ_U32(read_be16(&payload[11 + 2 * 2]), second_erpm);

    return true;
}

typedef struct {
    DataRecord *dr;
    uint32_t sample_rate;
} DataRecorderTinyBufferGuard;

static bool run_data_recorder_init(void *ctx) {
    DataRecorderTinyBufferGuard *guard = ctx;
    data_recorder_init(guard->dr, guard->sample_rate);
    return true;
}

static bool test_data_recorder_rejects_tiny_backing_buffer(void) {
    vesc_if_fake_reset();

    DataRecord dr = {0};
    uint8_t tiny_storage[sizeof(Sample) - 1] = {0};
    vesc_if_fake_set_data_buffer(0xcafe1011u, tiny_storage, sizeof(tiny_storage));

    DataRecorderTinyBufferGuard guard = {.dr = &dr, .sample_rate = 100u};
    EXPECT_TRUE(test_expect_no_signal(SIGFPE, run_data_recorder_init, &guard));

    EXPECT_TRUE(!data_recorder_has_capability(&dr));
    EXPECT_TRUE(!dr.recording);
    EXPECT_TRUE(dr.sample_count == 0);

    return true;
}

static bool test_data_recorder_data_send_pauses_recording(void) {
    vesc_if_fake_reset();

    Sample storage[2] = {0};
    DataRecord dr;
    init_data_record(&dr, storage, 2, true, true, true, true, 1, 100, 2);

    Data data = {0};
    data.state.state = STATE_RUNNING;
    data_recorder_sample(&dr, &data, 100u);
    EXPECT_TRUE(circular_buffer_size(&dr.buffer) == 1);

    size_t len = 0;
    const uint8_t *payload = SEND_DATA_RECORDER_REQUEST(&dr, len, 2, 2, 0, 0, 0, 0);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[1], 43u);
    EXPECT_TRUE(!dr.recording);

    return true;
}
