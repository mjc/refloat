#include "../c_support.hpp"

namespace {

TEST_CASE("smooth setpoint negative time constants", "[c][red]") {
    SmoothSetpoint st;
    smooth_setpoint_init(&st);

    smooth_setpoint_configure(&st, -0.1f, -0.2f, -0.3f, -0.4f, 10.0f, 20.0f, 30.0f, 40.0f, 100.0f);

    REQUIRE(isfinite(st.alpha));
    REQUIRE(isfinite(st.on_speed_alpha));
    REQUIRE(isfinite(st.off_speed_alpha));
    REQUIRE(isfinite(st.winddown_alpha));

    REQUIRE(st.alpha >= 0.0f);
    REQUIRE(st.on_speed_alpha >= 0.0f);
    REQUIRE(st.off_speed_alpha >= 0.0f);
    REQUIRE(st.winddown_alpha >= 0.0f);
}

TEST_CASE("frequency tracker nonpositive dt", "[c][red]") {
    FrequencyTracker ft;
    Time time = {.now = 1000u};
    frequency_tracker_init(&ft, 100.0f, &time);

    frequency_tracker_update(&ft, 0.01f);
    CHECK_FLOAT_NEAR(ft.dt, 10.0f);
    REQUIRE(isfinite(ft.frequency.value));
    REQUIRE(ft.frequency.value > 0.0f);

    float frequency_before = ft.frequency.value;
    frequency_tracker_update(&ft, 0.0f);
    CHECK_FLOAT_NEAR(ft.dt, 10.0f);
    CHECK_FLOAT_NEAR(ft.frequency.value, frequency_before);
    REQUIRE(isfinite(ft.frequency.value));
    REQUIRE(ft.frequency.value > 0.0f);

    frequency_tracker_update(&ft, -0.01f);
    CHECK_FLOAT_NEAR(ft.dt, 10.0f);
    CHECK_FLOAT_NEAR(ft.frequency.value, frequency_before);
    REQUIRE(isfinite(ft.frequency.value));
    REQUIRE(ft.frequency.value > 0.0f);
}

}  // namespace
