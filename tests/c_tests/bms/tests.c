#include "c_tests/test_support.h"

static CfgBMS default_bms_cfg(void) {
    return (CfgBMS){
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

typedef struct {
    float msg_age;
    float cell_lv;
    float cell_hv;
    int16_t cell_lt;
    int16_t cell_ht;
    int16_t bms_ht;
} BmsTelemetry;

typedef struct {
    const char *label;
    BmsTelemetry telemetry;
    uint32_t expected_fault_mask;
} BmsFaultCase;

static void bms_fixture_start(BmsFixture *fixture, uint32_t now_ticks) {
    *fixture = (BmsFixture){0};
    bms_init(&fixture->bms);
    fixture->cfg = default_bms_cfg();
    fixture->time = (Time){.now = now_ticks, .start_timer = now_ticks};
}

static void bms_set_telemetry(BMS *bms, BmsTelemetry telemetry) {
    bms->msg_age = telemetry.msg_age;
    bms->cell_lv = telemetry.cell_lv;
    bms->cell_hv = telemetry.cell_hv;
    bms->cell_lt = telemetry.cell_lt;
    bms->cell_ht = telemetry.cell_ht;
    bms->bms_ht = telemetry.bms_ht;
}

static bool bms_fault_case_matches(const BMS *bms, const BmsFaultCase *fault_case) {
    if (bms->fault_mask == fault_case->expected_fault_mask) {
        return true;
    }

    fprintf(stderr, "BMS telemetry case: %s\n", fault_case->label);
    test_report_u32_failure(
        __FILE__, __LINE__, fault_case->label, bms->fault_mask, fault_case->expected_fault_mask
    );
    return false;
}

static bool test_bms_reports_telemetry_faults(void) {
    static const BmsFaultCase cases[] = {
        {"valid telemetry", {0.0f, 3.70f, 3.75f, 20, 25, 30}, BMSF_NONE},
        {"cell voltage below safe minimum",
         {0.0f, 2.40f, 2.45f, 20, 25, 30},
         1u << (BMSF_CELL_UNDER_VOLTAGE - 1)},
        {"cell voltage above safe maximum",
         {0.0f, 4.25f, 4.30f, 20, 25, 30},
         1u << (BMSF_CELL_OVER_VOLTAGE - 1)},
        {"cell temperature below safe minimum",
         {0.0f, 3.70f, 3.75f, -11, 25, 30},
         1u << (BMSF_CELL_UNDER_TEMP - 1)},
        {"cell temperature above safe maximum",
         {0.0f, 3.70f, 3.75f, 20, 61, 30},
         1u << (BMSF_CELL_OVER_TEMP - 1)},
        {"BMS temperature above safe maximum",
         {0.0f, 3.70f, 3.75f, 20, 25, 71},
         1u << (BMSF_OVER_TEMP - 1)},
        {"cells differ by more than 100 mV",
         {0.0f, 3.70f, 3.801f, 20, 25, 30},
         1u << (BMSF_CELL_BALANCE - 1)},
        {"cells differ by less than 100 mV", {0.0f, 3.70f, 3.799f, 20, 25, 30}, BMSF_NONE},
        {"configured temperature limits", {0.0f, 3.70f, 3.75f, -10, 60, 70}, BMSF_NONE},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        BmsFixture fixture;
        bms_fixture_start(&fixture, 100000u);
        bms_set_telemetry(&fixture.bms, cases[index].telemetry);
        bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
        if (!bms_fault_case_matches(&fixture.bms, &cases[index])) {
            return false;
        }
    }

    return true;
}

static bool test_bms_timeout_and_configuration_contract(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 50000u);

    bms_set_telemetry(&fixture.bms, (BmsTelemetry){5.0f, 3.7f, 3.75f, 20, 25, 30});
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

    fixture.cfg.cell_ht_threshold = 0;
    fixture.cfg.bms_ht_threshold = 0;
    bms_set_telemetry(&fixture.bms, (BmsTelemetry){0.0f, 3.8f, 3.85f, -20, 80, 80});
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    fixture.cfg.enabled = false;
    fixture.bms.fault_mask = 0xffffffffu;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_valid_telemetry_stays_clear(void) {
    uint32_t seed = 0x0b5afeu;

    for (size_t case_index = 0; case_index < 256; ++case_index) {
        BmsFixture fixture;
        bms_fixture_start(&fixture, 50000u);

        float cell_lv = test_random_range(&seed, 3.0f, 3.95f);
        float cell_delta = test_random_range(&seed, 0.0f, 0.09f);
        fixture.bms.cell_lv = cell_lv;
        fixture.bms.cell_hv = cell_lv + cell_delta;
        fixture.bms.cell_lt = test_random_range(&seed, -9.0f, 30.0f);
        fixture.bms.cell_ht = test_random_range(&seed, 20.0f, 59.0f);
        fixture.bms.bms_ht = test_random_range(&seed, 20.0f, 69.0f);
        fixture.bms.msg_age = 0.0f;

        bms_update(&fixture.bms, &fixture.cfg, &fixture.time);

        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, fixture.bms.fault_mask == BMSF_NONE);
    }

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
    EXPECT_TRUE(fixture.bms.fault_mask == BMSF_NONE);

    return true;
}

static bool test_bms_is_fault_none_is_false(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 0u);

    fixture.bms.fault_mask = BMSF_NONE;
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_NONE));

    fixture.bms.fault_mask = 0xffffffffu;
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_NONE));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, (BMSFaultCode) 8));
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, (BMSFaultCode) 99));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CONNECTION));
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    return true;
}

static bool test_bms_rejects_hostile_voltage_thresholds(void) {
    BmsFixture fixture;
    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 2.0f;
    fixture.bms.cell_hv = 2.1f;
    fixture.cfg.cell_lv_threshold = -1.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_VOLTAGE));

    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 3.700f;
    fixture.bms.cell_hv = 3.705f;
    fixture.cfg.cell_balance_threshold = -1.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(!bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    return true;
}

static bool test_bms_bounds_hostile_upper_thresholds(void) {
    BmsFixture fixture;

    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 3.8f;
    fixture.bms.cell_hv = 4.6f;
    fixture.cfg.cell_hv_threshold = 30.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_OVER_VOLTAGE));

    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 2.8f;
    fixture.bms.cell_hv = 4.2f;
    fixture.cfg.cell_balance_threshold = 30.0f;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_BALANCE));

    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 3.8f;
    fixture.bms.cell_hv = 3.8f;
    fixture.bms.cell_ht = 61;
    fixture.cfg.cell_ht_threshold = INT8_MAX;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_OVER_TEMP));

    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 3.8f;
    fixture.bms.cell_hv = 3.8f;
    fixture.bms.cell_lt = -21;
    fixture.cfg.cell_ht_threshold = 60;
    fixture.cfg.cell_lt_threshold = INT8_MIN;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_CELL_UNDER_TEMP));

    bms_fixture_start(&fixture, 100000u);
    fixture.bms.msg_age = 0.0f;
    fixture.bms.cell_lv = 3.8f;
    fixture.bms.cell_hv = 3.8f;
    fixture.bms.bms_ht = 81;
    fixture.cfg.bms_ht_threshold = INT8_MAX;
    bms_update(&fixture.bms, &fixture.cfg, &fixture.time);
    EXPECT_TRUE(bms_is_fault(&fixture.bms, BMSF_OVER_TEMP));

    return true;
}
