#include "leds.h"

void lcm_fakes_set_runtime_status(bool enabled, bool headlights_enabled);

void leds_init(Leds *leds) {
    if (leds) {
        *leds = (Leds) {0};
    }
}

void leds_setup(Leds *leds, CfgHwLeds *hw_cfg, const CfgLeds *cfg) {
    (void) leds;
    (void) hw_cfg;
    (void) cfg;
    lcm_fakes_set_runtime_status(true, true);
}

void leds_configure(Leds *leds, const CfgLeds *cfg) {
    (void) leds;
    (void) cfg;
}

void leds_set_enabled(Leds *leds, bool value) {
    (void) leds;
    lcm_fakes_set_runtime_status(value, leds_get_runtime_status(leds)->headlights_enabled);
}

void leds_set_headlights_enabled(Leds *leds, bool value) {
    (void) leds;
    lcm_fakes_set_runtime_status(leds_get_runtime_status(leds)->enabled, value);
}

void leds_update(
    Leds *leds, const State *state, const MotorData *motor, FootpadSensorState fs_state
) {
    (void) leds;
    (void) state;
    (void) motor;
    (void) fs_state;
}

void leds_destroy(Leds *leds) {
    (void) leds;
}
