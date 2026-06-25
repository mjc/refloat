#include "conf/buffer.h"
#include "conf/confparser.h"
#include "data.h"
#include "vesc_if_fake.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "test_runner.h"

void refloat_main_fatal_error_terminate(void);
bool init(lib_info *info);

typedef struct {
    lib_info info;
    Data *data;
} MainProtocolFixture;

static bool main_protocol_fixture_start(MainProtocolFixture *fixture) {
    vesc_if_fake_reset();

    if (!init(&fixture->info)) {
        return false;
    }
    if (fixture->info.arg == NULL || fixture->info.stop_fun == NULL) {
        return false;
    }
    vesc_if_fake_set_arg(fixture->info.arg);

    fixture->data = fixture->info.arg;
    return true;
}

static void main_protocol_stop_info(const lib_info *info) {
    info->stop_fun(info->arg);
}

#include "c_tests/main_protocol/main_gnss_protocol.c"

int main(void) {
    const TestCase tests[] = {
        TEST_CASE("main gnss protocol", test_main_gnss_protocol),
    };

    RUN_TEST_SUITE("main protocol summary", tests);
}
