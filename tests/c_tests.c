#include "footpad_sensor.h"
#include "frequency_tracker.h"
#include "time.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "test_runner.h"

#include "c_tests/footpad_sensor/tests.c"
#include "c_tests/frequency_tracker/tests.c"

int main(void) {
    const TestCase tests[] = {
        TEST_CASE("footpad sensor", test_footpad_sensor),
        RED_XFAIL_CASE("frequency tracker nonpositive dt", test_frequency_tracker_nonpositive_dt,
                "red test: frequency_tracker_update divides by dt directly, so zero or negative "
                "loop deltas can feed infinity or invalid negative frequencies into the EMA "
                "instead of being ignored or clamped to a safe positive sample"),
    };

    RUN_TEST_SUITE("summary", tests);
}
