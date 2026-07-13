#include <cstddef>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include "vesc_if_fake.h"

bool init(lib_info *info);
void refloat_main_fatal_error_terminate(void);
}

static void reset_main_fakes() {
    vesc_if_fake_reset();
}

struct MainFixture {
    lib_info info{};
    bool active{false};

    void setup() {
        reset_main_fakes();
        info = {};
        REQUIRE(init(&info));
        REQUIRE(info.arg != nullptr);
        REQUIRE(info.stop_fun != nullptr);
        vesc_if_fake_set_arg(info.arg);
        active = true;
    }

    void stop() {
        if (active) {
            info.stop_fun(info.arg);
            active = false;
        }
    }

    ~MainFixture() {
        stop();
    }
};

TEST_CASE("main lifecycle initializes and stops cleanly", "[main][lifecycle]") {
    MainFixture fixture;
    fixture.setup();

    const std::size_t malloc_calls = vesc_if_fake_malloc_calls();
    CHECK(malloc_calls > 0);
    CHECK(vesc_if_fake_spawn_calls() == 2u);
    CHECK(vesc_if_fake_imu_set_read_callback_calls() == 1u);
    CHECK(vesc_if_fake_conf_custom_add_config_calls() == 1u);
    CHECK(vesc_if_fake_set_app_data_handler_calls() == 1u);
    CHECK(vesc_if_fake_lbm_add_extension_calls() == 2u);
    CHECK(vesc_if_fake_conf_custom_clear_configs_calls() == 0u);
    CHECK(vesc_if_fake_request_terminate_calls() == 0u);

    const std::size_t free_calls_before_stop = vesc_if_fake_free_calls();
    fixture.stop();

    CHECK(vesc_if_fake_imu_set_read_callback_calls() == 2u);
    CHECK(vesc_if_fake_set_app_data_handler_calls() == 2u);
    CHECK(vesc_if_fake_conf_custom_clear_configs_calls() == 1u);
    CHECK(vesc_if_fake_request_terminate_calls() == 2u);
    CHECK(vesc_if_fake_free_calls() > free_calls_before_stop);
    CHECK(vesc_if_fake_free_calls() <= malloc_calls);
}

TEST_CASE("main fatal error terminate runs the stop path", "[main][lifecycle]") {
    MainFixture fixture;
    fixture.setup();

    const std::size_t malloc_calls = vesc_if_fake_malloc_calls();
    const std::size_t free_calls_before_stop = vesc_if_fake_free_calls();

    refloat_main_fatal_error_terminate();
    fixture.active = false;

    CHECK(vesc_if_fake_imu_set_read_callback_calls() == 2u);
    CHECK(vesc_if_fake_set_app_data_handler_calls() == 2u);
    CHECK(vesc_if_fake_conf_custom_clear_configs_calls() == 1u);
    CHECK(vesc_if_fake_request_terminate_calls() == 2u);
    CHECK(vesc_if_fake_free_calls() > free_calls_before_stop);
    CHECK(vesc_if_fake_free_calls() <= malloc_calls);
}
