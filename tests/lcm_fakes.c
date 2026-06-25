#include "leds.h"
#include "vesc_c_if.h"

static LedsRuntimeStatus runtime_status;

void lcm_fakes_set_runtime_status(bool enabled, bool headlights_enabled) {
    runtime_status.enabled = enabled;
    runtime_status.headlights_enabled = headlights_enabled;
}

const LedsRuntimeStatus *leds_get_runtime_status(const Leds *leds) {
    (void) leds;
    return &runtime_status;
}

void fatal_error_terminate(const char *reason) {
    (void) reason;
}
