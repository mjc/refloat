#include "c_tests/test_support.h"

static bool test_state_transitions_and_compatibility(void) {
    static const struct {
        StopCondition stop;
        uint8_t compat;
    } stops[] = {
        {STOP_NONE, 11},
        {STOP_PITCH, 6},
        {STOP_ROLL, 7},
        {STOP_SWITCH_HALF, 8},
        {STOP_SWITCH_FULL, 9},
        {STOP_REVERSE_STOP, 12},
        {STOP_QUICKSTOP, 13},
    };
    static const struct {
        SetpointAdjustmentType sat;
        uint8_t compat;
    } adjustments[] = {
        {SAT_CENTERING, 0},
        {SAT_REVERSESTOP, 1},
        {SAT_NONE, 2},
        {SAT_PB_DUTY, 3},
        {SAT_PB_HIGH_VOLTAGE, 4},
        {SAT_PB_LOW_VOLTAGE, 5},
        {SAT_PB_TEMPERATURE, 6},
        {SAT_PB_SPEED, 7},
        {SAT_PB_ERROR, 8},
    };
    State state = {0};

    state_init(&state);
    EXPECT_TRUE(state.state == STATE_STARTUP);
    EXPECT_TRUE(state.mode == MODE_NORMAL);
    EXPECT_TRUE(state.sat == SAT_NONE);
    EXPECT_EQ_U32(state_compat(&state), 0u);
    EXPECT_EQ_U32(sat_compat(&state), 2u);

    for (size_t i = 0; i < sizeof(stops) / sizeof(stops[0]); ++i) {
        state_stop(&state, stops[i].stop);
        EXPECT_TRUE(state.state == STATE_READY);
        EXPECT_EQ_U32(state_compat(&state), stops[i].compat);
    }

    state_engage(&state);
    EXPECT_TRUE(state.state == STATE_RUNNING);
    EXPECT_TRUE(state.sat == SAT_CENTERING);
    EXPECT_TRUE(state.stop_condition == STOP_NONE);

    state.sat = SAT_PB_HIGH_VOLTAGE;
    EXPECT_EQ_U32(state_compat(&state), 2u);
    state.sat = SAT_PB_DUTY;
    state.wheelslip = true;
    EXPECT_EQ_U32(state_compat(&state), 3u);
    state.wheelslip = false;
    state.darkride = true;
    EXPECT_EQ_U32(state_compat(&state), 4u);
    state.darkride = false;
    state.mode = MODE_FLYWHEEL;
    EXPECT_EQ_U32(state_compat(&state), 5u);

    state.wheelslip = true;
    state_flywheel_off(&state);
    EXPECT_TRUE(state.state == STATE_READY);
    EXPECT_FALSE(state.wheelslip);

    state_init(&state);
    state_flywheel_off(&state);
    EXPECT_TRUE(state.state == STATE_STARTUP);
    state_set_disabled(&state, true);
    EXPECT_TRUE(state.state == STATE_DISABLED);
    EXPECT_EQ_U32(state_compat(&state), 15u);
    state_set_disabled(&state, false);
    EXPECT_TRUE(state.state == STATE_STARTUP);
    state_engage(&state);
    state_set_disabled(&state, true);
    EXPECT_TRUE(state.state == STATE_RUNNING);

    state.charging = true;
    EXPECT_EQ_U32(state_compat(&state), 14u);
    for (size_t i = 0; i < sizeof(adjustments) / sizeof(adjustments[0]); ++i) {
        state.sat = adjustments[i].sat;
        EXPECT_EQ_U32(sat_compat(&state), adjustments[i].compat);
    }

    state.charging = false;
    state.state = (RunState) 99;
    state.sat = (SetpointAdjustmentType) 99;
    EXPECT_EQ_U32(state_compat(&state), 0u);
    EXPECT_EQ_U32(sat_compat(&state), 0u);

    return true;
}

static bool test_time_updates_and_clock_fallback(void) {
    Time time = {0};

    vesc_if_fake_reset();
    vesc_if_fake_set_ticks(1000u);
    time_init(&time);
    EXPECT_EQ_U32(time.now, 1000u);
    EXPECT_EQ_U32(time.start_timer, 1000u);
    EXPECT_EQ_U32(time.engage_timer, 1000u);
    EXPECT_EQ_U32(time.idle_timer, 1000u);
    EXPECT_TRUE(timer_older(&time, time.disengage_timer, 59.0f));

    vesc_if_fake_set_ticks(1200u);
    time_update(&time, STATE_RUNNING);
    EXPECT_EQ_U32(time.now, 1200u);
    EXPECT_EQ_U32(time.disengage_timer, 1200u);
    EXPECT_EQ_U32(time.idle_timer, 1200u);

    vesc_if_fake_set_ticks(1300u);
    time_update(&time, STATE_READY);
    EXPECT_EQ_U32(time.now, 1300u);
    EXPECT_EQ_U32(time.disengage_timer, 1200u);
    EXPECT_EQ_U32(time.idle_timer, 1200u);

    fake_vesc_if.system_time_ticks = NULL;
    vesc_if_fake_set_seconds(2.0f);
    time_init(&time);
    EXPECT_EQ_U32(time.now, (systime_t) (2.0f * SYSTEM_TICK_RATE_HZ));

    return true;
}
