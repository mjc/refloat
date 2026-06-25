#pragma once

#include "vesc_c_if.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    vesc_c_if iface;
    uint8_t package_data[128];
} FakeVescIfStorage;

extern FakeVescIfStorage fake_vesc_if_storage;

#define fake_vesc_if (fake_vesc_if_storage.iface)

#undef VESC_IF
#define VESC_IF (&fake_vesc_if)

void vesc_if_fake_reset(void);
void vesc_if_fake_set_data_buffer(uint32_t magic, uint8_t *buffer, size_t length);
void vesc_if_fake_set_ticks(systime_t ticks);
void vesc_if_fake_set_seconds(float seconds);
void vesc_if_fake_set_output_disabled(bool disabled);
void vesc_if_fake_fill_next_malloc(uint8_t value);
void vesc_if_fake_fail_next_malloc(void);
void vesc_if_fake_fail_malloc_call(size_t call_number);
void vesc_if_fake_fail_spawn_call(size_t call_number);
void vesc_if_fake_set_analog(float adc1, float adc2);
void vesc_if_fake_set_ppm(float value, float age_s);
void vesc_if_fake_set_remote(float js_y, float age_s);
void vesc_if_fake_set_imu(float pitch, float roll, float yaw, float gyro_x, float gyro_y, float gyro_z);
void vesc_if_fake_set_imu_quaternions(float q0, float q1, float q2, float q3);
void vesc_if_fake_set_battery_level(float level);
void vesc_if_fake_set_gnss(double lat, double lon, float height, float speed, float hdop, systime_t last_update);
void vesc_if_fake_set_mc_gnss_missing(void);
void vesc_if_fake_set_mc_gnss_returns_null(bool returns_null);
void vesc_if_fake_set_arg(void *arg);
void vesc_if_fake_invoke_app_data_handler(unsigned char *data, unsigned int len);
void vesc_if_fake_set_cfg_float(CFG_PARAM param, float value);
void vesc_if_fake_set_cfg_int(CFG_PARAM param, int value);
void vesc_if_fake_set_motor_telemetry(
    float rpm,
    float speed,
    float distance,
    float current,
    float dir_current,
    float duty,
    float batt_current,
    float voltage,
    float fet_temp,
    float motor_temp
);
void vesc_if_fake_set_fault(mc_fault_code fault);
#define FAKE_COUNTER_ACCESSOR(field) size_t vesc_if_fake_##field(void)
FAKE_COUNTER_ACCESSOR(malloc_calls);
FAKE_COUNTER_ACCESSOR(free_calls);
FAKE_COUNTER_ACCESSOR(spawn_calls);
FAKE_COUNTER_ACCESSOR(request_terminate_calls);
FAKE_COUNTER_ACCESSOR(set_app_data_handler_calls);
FAKE_COUNTER_ACCESSOR(lbm_add_extension_calls);
FAKE_COUNTER_ACCESSOR(conf_custom_add_config_calls);
FAKE_COUNTER_ACCESSOR(conf_custom_clear_configs_calls);
FAKE_COUNTER_ACCESSOR(imu_set_read_callback_calls);
FAKE_COUNTER_ACCESSOR(set_pad_mode_calls);
FAKE_COUNTER_ACCESSOR(plot_init_calls);
FAKE_COUNTER_ACCESSOR(plot_add_graph_calls);
FAKE_COUNTER_ACCESSOR(plot_set_graph_calls);
FAKE_COUNTER_ACCESSOR(plot_send_points_calls);
FAKE_COUNTER_ACCESSOR(timeout_reset_calls);
FAKE_COUNTER_ACCESSOR(mc_set_current_calls);
FAKE_COUNTER_ACCESSOR(mc_set_brake_current_calls);
FAKE_COUNTER_ACCESSOR(mc_set_duty_calls);
FAKE_COUNTER_ACCESSOR(mc_set_current_off_delay_calls);
FAKE_COUNTER_ACCESSOR(foc_play_tone_calls);
FAKE_COUNTER_ACCESSOR(mc_gnss_calls);
#undef FAKE_COUNTER_ACCESSOR

#define FAKE_VALUE_ACCESSOR(type, field) type vesc_if_fake_##field(void)
FAKE_VALUE_ACCESSOR(void *, last_pad_gpio);
FAKE_VALUE_ACCESSOR(uint32_t, last_pad_pin);
FAKE_VALUE_ACCESSOR(uint32_t, last_pad_mode);
FAKE_VALUE_ACCESSOR(int, last_plot_graph);
FAKE_VALUE_ACCESSOR(float, last_plot_x);
FAKE_VALUE_ACCESSOR(float, last_plot_y);
FAKE_VALUE_ACCESSOR(float, last_current);
FAKE_VALUE_ACCESSOR(float, last_brake_current);
FAKE_VALUE_ACCESSOR(float, last_duty);
FAKE_VALUE_ACCESSOR(float, last_current_off_delay);
FAKE_VALUE_ACCESSOR(int, last_foc_channel);
FAKE_VALUE_ACCESSOR(float, last_foc_frequency);
FAKE_VALUE_ACCESSOR(float, last_foc_voltage);
#undef FAKE_VALUE_ACCESSOR

const uint8_t *vesc_if_fake_last_app_data(size_t *len);
