static CfgBMS default_bms_cfg(void) {
    return (CfgBMS) {
        .enabled = true,
        .cell_lv_threshold = 2.5f,
        .cell_hv_threshold = 4.2f,
        .cell_balance_threshold = 0.1f,
        .cell_lt_threshold = -10,
        .cell_ht_threshold = 60,
        .bms_ht_threshold = 70,
    };
}

static bool test_bms_faults(void) {
    BMS bms;
    bms_init(&bms);
    EXPECT_FLOAT_NEAR(bms.msg_age, 42.0f);
    EXPECT_TRUE(bms.fault_mask == BMSF_NONE);

    CfgBMS cfg = default_bms_cfg();

    Time time = {.now = 100000u, .start_timer = 100000u};

    bms.msg_age = 10.0f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CONNECTION));

    timer_expire(&time, &time.start_timer, 6.0f);
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CONNECTION));

    bms.msg_age = 0.0f;
    bms.cell_lv = 2.0f;
    bms.cell_hv = 4.3f;
    bms.cell_lt = -20;
    bms.cell_ht = 80;
    bms.bms_ht = 80;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_UNDER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_OVER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_BALANCE));

    cfg.cell_ht_threshold = 0;
    cfg.bms_ht_threshold = 0;
    bms.cell_lt = -20;
    bms.cell_ht = 80;
    bms.bms_ht = 80;
    bms.cell_lv = 3.8f;
    bms.cell_hv = 3.85f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_BALANCE));

    cfg.enabled = false;
    bms.fault_mask = 0xffffffffu;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_threshold_boundaries(void) {
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
    EXPECT_TRUE(bms.fault_mask == BMSF_NONE);

    bms.msg_age = 5.001f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CONNECTION));

    time.now += 1u;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CONNECTION));

    bms.msg_age = 0.0f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CONNECTION));

    bms.cell_lv = cfg.cell_lv_threshold;
    bms.cell_hv = cfg.cell_hv_threshold;
    bms.cell_lt = cfg.cell_lt_threshold;
    bms.cell_ht = cfg.cell_ht_threshold;
    bms.bms_ht = cfg.bms_ht_threshold;
    bms.msg_age = 0.0f;
    cfg.cell_balance_threshold = 2.0f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_UNDER_VOLTAGE));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_OVER_VOLTAGE));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_BALANCE));

    cfg.cell_balance_threshold = 0.1f;
    bms.cell_lv = 3.7f;
    bms.cell_hv = 3.8f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_CELL_BALANCE));

    bms.cell_hv += 0.001f;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_BALANCE));

    return true;
}

static bool test_bms_faults_clear_on_recovery(void) {
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
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_UNDER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_OVER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_BALANCE));

    bms.cell_lv = 3.70f;
    bms.cell_hv = 3.75f;
    bms.cell_lt = 20;
    bms.cell_ht = 25;
    bms.bms_ht = 30;
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_startup_grace_waits_for_first_sample(void) {
    BMS bms;
    bms_init(&bms);

    CfgBMS cfg = default_bms_cfg();

    Time time = {.now = 100000u, .start_timer = 100000u};
    bms_update(&bms, &cfg, &time);
    EXPECT_TRUE(bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_is_fault_none_is_false(void) {
    BMS bms;
    bms_init(&bms);

    bms.fault_mask = BMSF_NONE;
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_NONE));

    bms.fault_mask = 0xffffffffu;
    EXPECT_TRUE(!bms_is_fault(&bms, BMSF_NONE));
    EXPECT_TRUE(!bms_is_fault(&bms, (BMSFaultCode) 8));
    EXPECT_TRUE(!bms_is_fault(&bms, (BMSFaultCode) 99));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CONNECTION));
    EXPECT_TRUE(bms_is_fault(&bms, BMSF_CELL_BALANCE));

    return true;
}
