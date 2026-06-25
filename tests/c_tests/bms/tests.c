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

typedef struct {
    BMS bms;
    CfgBMS cfg;
    Time time;
} BmsFixture;

static void bms_fixture_start(BmsFixture *fixture, uint32_t now_ticks) {
    *fixture = (BmsFixture) {0};
    bms_init(&fixture->bms);
    fixture->cfg = default_bms_cfg();
    fixture->time = (Time) {.now = now_ticks, .start_timer = now_ticks};
}

static bool test_bms_faults(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 100000u);
    EXPECT_FLOAT_NEAR(fixture.bms.msg_age, 42.0f);
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    fixture.bms.msg_age = 10.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CONNECTION));

    timer_expire(&fixture.time, &fixture.time.start_timer, 6.0f);
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CONNECTION));

    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 2.0f;
    fixture.bms.cell_hv = 4.3f;
    fixture.bms.cell_lt = -20;
    fixture.bms.cell_ht = 80;
    fixture.bms.bms_ht = 80;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_OVER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    fixture.cfg.cell_ht_threshold = 0;
    fixture.cfg.bms_ht_threshold = 0;
    fixture.bms.cell_lt = -20;
    fixture.bms.cell_ht = 80;
    fixture.bms.bms_ht = 80;
    fixture.bms.cell_lv = 3.8f;
    fixture.bms.cell_hv = 3.85f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    fixture.cfg.enabled = false;
    fixture.bms.fault_mask = 0xffffffffu;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_threshold_boundaries(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 50000u);

    fixture.bms.cell_lv = 3.7f;
    fixture.bms.cell_hv = 3.75f;
    fixture.bms.cell_lt = 20;
    fixture.bms.cell_ht = 25;
    fixture.bms.bms_ht = 30;
    fixture.bms.msg_age = 5.0f;
    timer_expire(&fixture.time, &fixture.time.start_timer, 5.0f);
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    fixture.bms.msg_age = 5.001f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CONNECTION));

    fixture.time.now += 1u;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CONNECTION));

    fixture.bms.msg_age = 0.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CONNECTION));

    fixture.bms.cell_lv = fixture.cfg.cell_lv_threshold;
    fixture.bms.cell_hv = fixture.cfg.cell_hv_threshold;
    fixture.bms.cell_lt = fixture.cfg.cell_lt_threshold;
    fixture.bms.cell_ht = fixture.cfg.cell_ht_threshold;
    fixture.bms.bms_ht = fixture.cfg.bms_ht_threshold;
    fixture.bms.msg_age = 0.0f;
    fixture.cfg.cell_balance_threshold = 2.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_VOLTAGE));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_OVER_VOLTAGE));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    fixture.cfg.cell_balance_threshold = 0.1f;
    fixture.bms.cell_lv = 3.7f;
    fixture.bms.cell_hv = 3.8f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    fixture.bms.cell_hv += 0.001f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    return true;
}

static bool test_bms_faults_clear_on_recovery(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 100000u);
    timer_expire(&fixture.time, &fixture.time.start_timer, 6.0f);

    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 2.0f;
    fixture.bms.cell_hv = 4.4f;
    fixture.bms.cell_lt = -20;
    fixture.bms.cell_ht = 80;
    fixture.bms.bms_ht = 90;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_OVER_VOLTAGE));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_TEMP));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_OVER_TEMP));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    fixture.bms.cell_lv = 3.70f;
    fixture.bms.cell_hv = 3.75f;
    fixture.bms.cell_lt = 20;
    fixture.bms.cell_ht = 25;
    fixture.bms.bms_ht = 30;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_startup_grace_waits_for_first_sample(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 100000u);
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    RED_EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_is_fault_none_is_false(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 0u);

    fixture.bms.fault_mask = BMSF_NONE;
    RED_EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_NONE));

    fixture.bms.fault_mask = 0xffffffffu;
    RED_EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_NONE));
    RED_EXPECT_TRUE(!bms_is_fault(&fixture.bms, (BMSFaultCode) 8));
    RED_EXPECT_TRUE(!bms_is_fault(&fixture.bms, (BMSFaultCode) 99));
    RED_EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CONNECTION));
    RED_EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    return true;
}
