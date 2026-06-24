static bool test_alert_tracker_and_fatal_reset(void) {
    AlertTracker at;
    Time time = {.now = 1000u};
    RefloatConfig cfg = {.persistent_fatal_error = false};

    alert_tracker_init(&at);
    EXPECT_TRUE(at.persistent_fatal_error);
    EXPECT_TRUE(at.active_alert_mask == 0);
    EXPECT_TRUE(at.new_active_alert_mask == 0);
    EXPECT_TRUE(at.fatal_error == false);

    alert_tracker_configure(&at, &cfg);
    EXPECT_TRUE(!at.persistent_fatal_error);

    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    EXPECT_TRUE(at.fatal_error);
    EXPECT_TRUE(at.fw_fault_code == FAULT_CODE_ABS_OVER_CURRENT);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));

    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    EXPECT_TRUE(at.new_active_alert_mask == 0);
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == 1);
    AlertRecord record = {0};
    circular_buffer_get(&at.alert_buffer, 0, &record);
    EXPECT_TRUE(record.active);
    EXPECT_TRUE(record.id == ALERT_FW_FAULT);
    EXPECT_TRUE(record.code == FAULT_CODE_ABS_OVER_CURRENT);
    EXPECT_TRUE(record.time == 1000u);

    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == 1);

    time.now += 5u;
    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_OVER_TEMP_MOTOR);
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == 2);
    circular_buffer_get(&at.alert_buffer, 1, &record);
    EXPECT_TRUE(record.active);
    EXPECT_TRUE(record.code == FAULT_CODE_OVER_TEMP_MOTOR);
    EXPECT_TRUE(at.fw_fault_code == FAULT_CODE_OVER_TEMP_MOTOR);

    time.now += 10u;
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(at.fw_fault_code == FAULT_CODE_NONE);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == 3);
    circular_buffer_get(&at.alert_buffer, 2, &record);
    EXPECT_TRUE(!record.active);
    EXPECT_TRUE(record.id == ALERT_FW_FAULT);

    cfg.persistent_fatal_error = true;
    alert_tracker_configure(&at, &cfg);
    time.now += 10u;
    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    alert_tracker_finalize(&at, &time);
    time.now += 10u;
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(at.fatal_error);
    alert_tracker_clear_fatal(&at);
    EXPECT_TRUE(!at.fatal_error);

    for (uint8_t i = 0; i < ALERT_TRACKER_SIZE + 3; ++i) {
        time.now += 1u;
        alert_tracker_add(&at, &time, ALERT_FW_FAULT, i);
        alert_tracker_finalize(&at, &time);
    }
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == ALERT_TRACKER_SIZE);
    EXPECT_TRUE(alert_tracker_properties(ALERT_FW_FAULT)->type == ATYPE_FATAL);

    return true;
}

static bool test_alert_tracker_nonpersistent_fatal_clears_when_alert_ends(void) {
    AlertTracker at;
    Time time = {.now = 1000u};
    RefloatConfig cfg = {.persistent_fatal_error = false};

    alert_tracker_init(&at);
    alert_tracker_configure(&at, &cfg);

    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(at.fatal_error);
    EXPECT_TRUE(alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));

    time.now += 10u;
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    EXPECT_TRUE(!at.fatal_error);

    return true;
}

static bool test_alert_tracker_rejects_invalid_ids(void) {
    AlertTracker at;
    Time time = {.now = 2000u};

    alert_tracker_init(&at);

    alert_tracker_add(&at, &time, ALERT_NONE, 99);
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    EXPECT_TRUE(!at.fatal_error);
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == 0);

    time.now += 1u;
    alert_tracker_add(&at, &time, ALERT_LAST + 1, 99);
    alert_tracker_finalize(&at, &time);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    EXPECT_TRUE(!at.fatal_error);
    EXPECT_TRUE(circular_buffer_size(&at.alert_buffer) == 0);

    return true;
}
