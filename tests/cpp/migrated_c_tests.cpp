#include <catch2/catch_test_macros.hpp>

extern "C" bool refloat_run_migrated_c_test(const char *name);

TEST_CASE("frequency tracker nonpositive dt", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("frequency tracker nonpositive dt"));
}

TEST_CASE("footpad sensor", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("footpad sensor"));
}

TEST_CASE("charging timeout boundaries", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("charging timeout boundaries"));
}

TEST_CASE("charging signed payload and invalid frame edges", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("charging signed payload and invalid frame edges"));
}

TEST_CASE("remote branch cases", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("remote branch cases"));
}

TEST_CASE("remote uart and command timeout edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("remote uart and command timeout edges"));
}

TEST_CASE("remote deadband invert and idle move edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("remote deadband invert and idle move edges"));
}

TEST_CASE("remote deadband and age boundaries", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("remote deadband and age boundaries"));
}

TEST_CASE("remote rejects invalid deadband config", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("remote rejects invalid deadband config"));
}

TEST_CASE("remote move torque nonfinite dt", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("remote move torque nonfinite dt"));
}

TEST_CASE("imu update edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("imu update edges"));
}

TEST_CASE("imu flywheel roll wrap boundaries", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("imu flywheel roll wrap boundaries"));
}

TEST_CASE("bms faults", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("bms faults"));
}

TEST_CASE("bms threshold boundaries", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("bms threshold boundaries"));
}

TEST_CASE("bms faults clear on recovery", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("bms faults clear on recovery"));
}

TEST_CASE("bms startup grace waits for first sample", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("bms startup grace waits for first sample"));
}

TEST_CASE("bms is fault none is false", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("bms is fault none is false"));
}

TEST_CASE("led strip", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("led strip"));
}

TEST_CASE("led driver setup and color encoding", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("led driver setup and color encoding"));
}

TEST_CASE("led driver rejects invalid pin", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("led driver rejects invalid pin"));
}

TEST_CASE("led driver rejects invalid color order", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("led driver rejects invalid color order"));
}

TEST_CASE("led driver rejects oversized strip count", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("led driver rejects oversized strip count"));
}

TEST_CASE("led driver alternate pins and noop paths", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("led driver alternate pins and noop paths"));
}

TEST_CASE("led driver full brightness color orders", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("led driver full brightness color orders"));
}

TEST_CASE("data recorder requests", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder requests"));
}

TEST_CASE("data recorder experiment plot export", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder experiment plot export"));
}

TEST_CASE("data recorder request edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder request edges"));
}

TEST_CASE("data recorder decimation and sample flags", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder decimation and sample flags"));
}

TEST_CASE("data recorder sample rate recomputes decimation", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder sample rate recomputes decimation"));
}

TEST_CASE("data recorder status and data serialization", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder status and data serialization"));
}

TEST_CASE("data recorder rejects tiny backing buffer", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder rejects tiny backing buffer"));
}

TEST_CASE("data recorder data send pauses recording", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("data recorder data send pauses recording"));
}

TEST_CASE("booster and brake tilt branch cases", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("booster and brake tilt branch cases"));
}

TEST_CASE("brake tilt negative erpm downhill boundaries", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("brake tilt negative erpm downhill boundaries"));
}

TEST_CASE("booster threshold boundary edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("booster threshold boundary edges"));
}

TEST_CASE("booster threshold ramp and reset edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("booster threshold ramp and reset edges"));
}

TEST_CASE("atr branch cases", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("atr branch cases"));
}

TEST_CASE("atr threshold speedboost and reset edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("atr threshold speedboost and reset edges"));
}

TEST_CASE("atr zero accel ratio config", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("atr zero accel ratio config"));
}

TEST_CASE("balance filter nonfinite dt", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("balance filter nonfinite dt"));
}

TEST_CASE("torque tilt", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("torque tilt"));
}

TEST_CASE("torque tilt sign strength and filter edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("torque tilt sign strength and filter edges"));
}

TEST_CASE("torque tilt negative limit and regen edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("torque tilt negative limit and regen edges"));
}

TEST_CASE("motor control current brake and tone", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor control current brake and tone"));
}

TEST_CASE("motor control parking and tone edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor control parking and tone edges"));
}

TEST_CASE("motor control zero tone frequency", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("motor control zero tone frequency"));
}

TEST_CASE("motor control click lifecycle edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor control click lifecycle edges"));
}

TEST_CASE("motor control parking and moving threshold edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor control parking and moving threshold edges"));
}

TEST_CASE("motor data refresh update and alerts", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor data refresh update and alerts"));
}

TEST_CASE("motor data fallback limits and direction edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor data fallback limits and direction edges"));
}

TEST_CASE("motor data init alert and saturation edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor data init alert and saturation edges"));
}

TEST_CASE("motor data forward direction threshold edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor data forward direction threshold edges"));
}

TEST_CASE("motor data torque constant config edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("motor data torque constant config edges"));
}

TEST_CASE("motor data erpm speed conversion", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("motor data erpm speed conversion"));
}

TEST_CASE("motor data nonpositive dt", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("motor data nonpositive dt"));
}

TEST_CASE("turn tilt branch cases", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("turn tilt branch cases"));
}

TEST_CASE("turn tilt aggregate and boost edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("turn tilt aggregate and boost edges"));
}

TEST_CASE("turn tilt threshold boundary edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("turn tilt threshold boundary edges"));
}

TEST_CASE("turn tilt zero denominator config", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("turn tilt zero denominator config"));
}

TEST_CASE("turn tilt nonpositive dt", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("turn tilt nonpositive dt"));
}

TEST_CASE("reverse stop update paths", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("reverse stop update paths"));
}

TEST_CASE("reverse stop completion and timer edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("reverse stop completion and timer edges"));
}

TEST_CASE("reverse stop progress clear and completed distance edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("reverse stop progress clear and completed distance edges"));
}

TEST_CASE("alert tracker and fatal reset", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("alert tracker and fatal reset"));
}

TEST_CASE("alert tracker nonpersistent fatal clears when alert ends", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("alert tracker nonpersistent fatal clears when alert ends"));
}

TEST_CASE("alert tracker rejects invalid ids", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("alert tracker rejects invalid ids"));
}

TEST_CASE("konami sequence and timeout", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("konami sequence and timeout"));
}

TEST_CASE("konami boundary and idle inputs", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("konami boundary and idle inputs"));
}

TEST_CASE("konami single step sequence", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("konami single step sequence"));
}

TEST_CASE("haptic feedback patterns", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("haptic feedback patterns"));
}

TEST_CASE("haptic feedback gating and strength edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("haptic feedback gating and strength edges"));
}

TEST_CASE("haptic feedback shared strength scale", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("haptic feedback shared strength scale"));
}

TEST_CASE("haptic feedback pattern type change lockout", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("haptic feedback pattern type change lockout"));
}

TEST_CASE("haptic feedback type selection edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("haptic feedback type selection edges"));
}

TEST_CASE("haptic feedback error pattern pause edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("haptic feedback error pattern pause edges"));
}

TEST_CASE("sma growth transition edges", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("sma growth transition edges"));
}

TEST_CASE("sma allocation failure update", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("sma allocation failure update"));
}

TEST_CASE("circular buffer pop index", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("circular buffer pop index"));
}

TEST_CASE("lcm payload clamp", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("lcm payload clamp"));
}

TEST_CASE("smooth setpoint negative time constants", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("smooth setpoint negative time constants"));
}

TEST_CASE("lcm disabled responses are minimal", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("lcm disabled responses are minimal"));
}

TEST_CASE("lcm init configure and runtime brightness", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("lcm init configure and runtime brightness"));
}

TEST_CASE("lcm configure requires initialized led config", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("lcm configure requires initialized led config"));
}

TEST_CASE("lcm poll response pitch payload and name edges", "[c]") {
    REQUIRE(refloat_run_migrated_c_test("lcm poll response pitch payload and name edges"));
}

TEST_CASE("lcm poll request respects name length", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("lcm poll request respects name length"));
}

TEST_CASE("lcm poll response saturates byte fields", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("lcm poll response saturates byte fields"));
}

TEST_CASE("lcm battery response nonfinite values are stable", "[c][red]") {
    REQUIRE(refloat_run_migrated_c_test("lcm battery response nonfinite values are stable"));
}

