#include "vesc_if_fake.h"
#include "st_types.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * Host-only VESC boundary, audited against lukash/bldc
 * v6.06-rflt-xtras-0.1 (9e5a8ff0523e295294062052f63418a60f16dfcd) and
 * its vesc_c_if interface. This fake deliberately differs from firmware in
 * three ways:
 *
 * - spawn/sleep are deterministic lifecycle controls, not a scheduler;
 * - EEPROM is a bounded in-memory store with explicit failure injection; and
 * - setters inject callback observations directly, without claiming a UI or
 *   Bluetooth path produced them.
 *
 * Tests may use those controls to verify Refloat's response to a documented
 * callback or failure. They must not treat a fake-only value as a device bug;
 * reachability is recorded with the owning behavior test.
 */

enum {
    FAKE_VESC_PACKAGE_DATA_OFFSET = 2036
};

typedef struct {
    uint32_t magic;
    uint8_t *buffer;
    size_t length;
} FakeDataBufferInfo;

static struct {
    systime_t ticks;
    float seconds;
    uint32_t timer_ticks;
    size_t sleep_us_calls;
    bool terminate_after_sleep;
    bool imu_startup_done;
    bool output_disabled;
#define FAKE_COUNTER_FIELD(field) size_t field;
    FAKE_COUNTER_FIELD(malloc_calls)
    FAKE_COUNTER_FIELD(free_calls)
    FAKE_COUNTER_FIELD(spawn_calls)
    FAKE_COUNTER_FIELD(request_terminate_calls)
    FAKE_COUNTER_FIELD(set_app_data_handler_calls)
    FAKE_COUNTER_FIELD(lbm_add_extension_calls)
    FAKE_COUNTER_FIELD(conf_custom_add_config_calls)
    FAKE_COUNTER_FIELD(conf_custom_clear_configs_calls)
    FAKE_COUNTER_FIELD(imu_set_read_callback_calls)
    FAKE_COUNTER_FIELD(set_pad_mode_calls)
    FAKE_COUNTER_FIELD(plot_init_calls)
    FAKE_COUNTER_FIELD(plot_add_graph_calls)
    FAKE_COUNTER_FIELD(plot_set_graph_calls)
    FAKE_COUNTER_FIELD(plot_send_points_calls)
    FAKE_COUNTER_FIELD(timeout_reset_calls)
    FAKE_COUNTER_FIELD(mc_set_current_calls)
    FAKE_COUNTER_FIELD(mc_set_brake_current_calls)
    FAKE_COUNTER_FIELD(mc_set_duty_calls)
    FAKE_COUNTER_FIELD(mc_set_current_off_delay_calls)
    FAKE_COUNTER_FIELD(foc_play_tone_calls)
    FAKE_COUNTER_FIELD(mc_gnss_calls)
#undef FAKE_COUNTER_FIELD
#define FAKE_VALUE_FIELD(type, field) type field;
    FAKE_VALUE_FIELD(void *, last_pad_gpio)
    FAKE_VALUE_FIELD(uint32_t, last_pad_pin)
    FAKE_VALUE_FIELD(uint32_t, last_pad_mode)
    FAKE_VALUE_FIELD(int, last_plot_graph)
    FAKE_VALUE_FIELD(float, last_plot_x)
    FAKE_VALUE_FIELD(float, last_plot_y)
    FAKE_VALUE_FIELD(float, last_current)
    FAKE_VALUE_FIELD(float, last_brake_current)
    FAKE_VALUE_FIELD(float, last_duty)
    FAKE_VALUE_FIELD(float, last_current_off_delay)
    FAKE_VALUE_FIELD(int, last_foc_channel)
    FAKE_VALUE_FIELD(float, last_foc_frequency)
    FAKE_VALUE_FIELD(float, last_foc_voltage)
#undef FAKE_VALUE_FIELD
    bool malloc_fill_enabled;
    uint8_t malloc_fill_value;
    bool malloc_fail_enabled;
    size_t malloc_fail_call;
    float adc1;
    float adc2;
    float ppm;
    float ppm_age;
    remote_state remote;
    float imu_pitch;
    float imu_roll;
    float imu_yaw;
    float imu_gyro[3];
    float imu_quat[4];
    gnss_data gnss;
    bool gnss_returns_null;
    void *prog_arg;
    void (*app_data_handler)(unsigned char *data, unsigned int len);
    void (*imu_read_callback)(float *acc, float *gyro, float *mag, float dt);
    int (*get_custom_cfg)(uint8_t *data, bool is_default);
    bool (*set_custom_cfg)(uint8_t *data);
    int (*get_custom_cfg_xml)(uint8_t **data);
    extension_fptr ext_set_fw_version;
    extension_fptr ext_bms;
    size_t spawn_fail_call;
    size_t read_eeprom_var_calls;
    size_t store_eeprom_var_calls;
    bool eeprom_read_enabled;
    bool eeprom_store_enabled;
    size_t eeprom_read_fail_call;
    size_t eeprom_store_fail_call;
    size_t eeprom_store_discard_call;
    bool eeprom_store_discard;
    int last_eeprom_store_address;
    eeprom_var eeprom[512];
    float cfg_float[CFG_PARAM_foc_motor_flux_linkage + 1];
    int cfg_int[CFG_PARAM_foc_motor_flux_linkage + 1];
    uint64_t odometer;
    float amp_hours;
    float amp_hours_charged;
    float watt_hours;
    float watt_hours_charged;
    float motor_rpm;
    float motor_speed;
    float motor_distance;
    float motor_distance_abs;
    float motor_current;
    float motor_dir_current;
    float motor_duty;
    float motor_batt_current;
    float motor_voltage;
    float motor_fet_temp;
    float motor_temp;
    float battery_level;
    mc_fault_code fault;
    uint8_t app_data[512];
    size_t app_data_len;
} fake_state;

FakeVescIfStorage fake_vesc_if_storage;

#define FAKE_GETTER(type, name, field)                                                             \
    type name(void) {                                                                              \
        return fake_state.field;                                                                   \
    }
#define FAKE_FLOAT_GETTER(name, field) FAKE_GETTER(float, name, field)
#define FAKE_SIZE_GETTER(name, field) FAKE_GETTER(size_t, name, field)
#define FAKE_STATIC_GETTER(type, name, field)                                                      \
    static type fake_##name(void) {                                                                \
        return fake_state.field;                                                                   \
    }
#define FAKE_STATIC_FLOAT_GETTER(name, field) FAKE_STATIC_GETTER(float, name, field)
#define FAKE_STATIC_RESET_FLOAT_GETTER(name, field)                                                \
    static float fake_##name(bool reset) {                                                         \
        (void) reset;                                                                              \
        return fake_state.field;                                                                   \
    }
#define FAKE_COUNT_CALL(name, count_field)                                                         \
    static void fake_##name(void) {                                                                \
        ++fake_state.count_field;                                                                  \
    }
#define FAKE_RECORD_FLOAT(name, arg, count_field, field)                                           \
    static void fake_##name(float arg) {                                                           \
        ++fake_state.count_field;                                                                  \
        fake_state.field = arg;                                                                    \
    }
#define FAKE_SETTER(func, type, arg, field)                                                        \
    void vesc_if_fake_##func(type arg) {                                                           \
        fake_state.field = arg;                                                                    \
    }

static void map_test_hw_page(uintptr_t addr) {
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page = addr & ~((uintptr_t) page_size - 1u);

    void *mapped = mmap(
        (void *) page,
        (size_t) page_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE,
        -1,
        0
    );

    if (mapped == MAP_FAILED) {
        return;
    }
}

static void reset_test_hw(void) {
    map_test_hw_page(TEST_GPIOB_BASE);
    map_test_hw_page(TEST_GPIOC_BASE);
    map_test_hw_page(TEST_TIM3_BASE);
    map_test_hw_page(TEST_TIM4_BASE);
    map_test_hw_page(TEST_DMA1_STREAM0_BASE);
    map_test_hw_page(TEST_DMA1_STREAM2_BASE);
    map_test_hw_page(TEST_DMA1_STREAM3_BASE);
    map_test_hw_page(TEST_DMA1_BASE);
    map_test_hw_page(TEST_RCC_BASE);

    memset(GPIOB, 0, sizeof(*GPIOB));
    memset(GPIOC, 0, sizeof(*GPIOC));
    memset(TIM3, 0, sizeof(*TIM3));
    memset(TIM4, 0, sizeof(*TIM4));
    memset(DMA1_Stream0, 0, sizeof(*DMA1_Stream0));
    memset(DMA1_Stream2, 0, sizeof(*DMA1_Stream2));
    memset(DMA1_Stream3, 0, sizeof(*DMA1_Stream3));
    memset(DMA1, 0, sizeof(*DMA1));
    memset(RCC, 0, sizeof(*RCC));
}

static bool fake_app_is_output_disabled(void) {
    return fake_state.output_disabled;
}

FAKE_STATIC_FLOAT_GETTER(system_time, seconds)
FAKE_STATIC_GETTER(systime_t, system_time_ticks, ticks)

static int fake_printf(const char *fmt, ...) {
    (void) fmt;
    return 0;
}

static void *fake_malloc(size_t bytes) {
    ++fake_state.malloc_calls;
    if (fake_state.malloc_fail_enabled || fake_state.malloc_calls == fake_state.malloc_fail_call) {
        fake_state.malloc_fail_enabled = false;
        fake_state.malloc_fail_call = 0;
        return NULL;
    }
    void *ptr = malloc(bytes);
    if (ptr && fake_state.prog_arg == NULL) {
        fake_state.prog_arg = ptr;
    }
    if (ptr && fake_state.malloc_fill_enabled) {
        memset(ptr, fake_state.malloc_fill_value, bytes);
        fake_state.malloc_fill_enabled = false;
    }
    return ptr;
}

static void fake_free(void *ptr) {
    if (ptr) {
        ++fake_state.free_calls;
    }
    if (ptr == fake_state.prog_arg) {
        fake_state.prog_arg = NULL;
    }
    free(ptr);
}

static void fake_sleep_ms(uint32_t ms) {
    (void) ms;
}

static void fake_sleep_us(uint32_t us) {
    ++fake_state.sleep_us_calls;
    fake_state.timer_ticks += us;
}

static void fake_set_pad_mode(void *gpio, uint32_t pin, uint32_t mode) {
    ++fake_state.set_pad_mode_calls;
    fake_state.last_pad_gpio = gpio;
    fake_state.last_pad_pin = pin;
    fake_state.last_pad_mode = mode;
}

static void fake_request_terminate(lib_thread thd) {
    (void) thd;
    ++fake_state.request_terminate_calls;
}

static lib_thread fake_spawn(
    void (*fun)(void *arg), size_t stack_size, const char *name, void *arg
) {
    (void) fun;
    (void) stack_size;
    (void) name;
    (void) arg;
    ++fake_state.spawn_calls;
    if (fake_state.spawn_calls == fake_state.spawn_fail_call) {
        fake_state.spawn_fail_call = 0;
        return NULL;
    }
    static int fake_threads[4];
    return &fake_threads[fake_state.spawn_calls % 4];
}

static void **fake_get_arg(uint32_t prog_addr) {
    (void) prog_addr;
    return &fake_state.prog_arg;
}

static void fake_imu_set_read_callback(void (*func)(float *acc, float *gyro, float *mag, float dt)
) {
    fake_state.imu_read_callback = func;
    ++fake_state.imu_set_read_callback_calls;
}

static void fake_conf_custom_add_config(
    int (*get_cfg)(uint8_t *data, bool is_default),
    bool (*set_cfg)(uint8_t *data),
    int (*get_cfg_xml)(uint8_t **data)
) {
    fake_state.get_custom_cfg = get_cfg;
    fake_state.set_custom_cfg = set_cfg;
    fake_state.get_custom_cfg_xml = get_cfg_xml;
    ++fake_state.conf_custom_add_config_calls;
}

static void fake_conf_custom_clear_configs(void) {
    ++fake_state.conf_custom_clear_configs_calls;
}

static bool fake_lbm_add_extension(char *name, extension_fptr fun) {
    if (strcmp(name, "ext-set-fw-version") == 0) {
        fake_state.ext_set_fw_version = fun;
    } else if (strcmp(name, "ext-bms") == 0) {
        fake_state.ext_bms = fun;
    }
    ++fake_state.lbm_add_extension_calls;
    return true;
}

static float fake_lbm_dec_as_float(lbm_value value) {
    return (float) (int32_t) value;
}

static int32_t fake_lbm_dec_as_i32(lbm_value value) {
    return (int32_t) value;
}

static void fake_send_app_data(unsigned char *data, unsigned int len) {
    if (len > sizeof(fake_state.app_data)) {
        len = sizeof(fake_state.app_data);
    }
    memcpy(fake_state.app_data, data, len);
    fake_state.app_data_len = len;
}

static bool fake_set_app_data_handler(void (*func)(unsigned char *data, unsigned int len)) {
    ++fake_state.set_app_data_handler_calls;
    fake_state.app_data_handler = func;
    return true;
}

static void fake_invoke_app_data_handler(unsigned char *data, unsigned int len) {
    if (fake_state.app_data_handler) {
        fake_state.app_data_handler(data, len);
    }
}

static void fake_plot_init(const char *namex, const char *namey) {
    (void) namex;
    (void) namey;
    ++fake_state.plot_init_calls;
}

static void fake_plot_add_graph(const char *name) {
    (void) name;
    ++fake_state.plot_add_graph_calls;
}

static void fake_plot_set_graph(int graph) {
    ++fake_state.plot_set_graph_calls;
    fake_state.last_plot_graph = graph;
}

static void fake_plot_send_points(float x, float y) {
    ++fake_state.plot_send_points_calls;
    fake_state.last_plot_x = x;
    fake_state.last_plot_y = y;
}

FAKE_COUNT_CALL(timeout_reset, timeout_reset_calls)

static bool fake_io_set_mode(VESC_PIN pin, VESC_PIN_MODE mode) {
    (void) pin;
    (void) mode;
    return true;
}

static bool fake_io_write(VESC_PIN pin, int state) {
    (void) pin;
    (void) state;
    return true;
}

FAKE_RECORD_FLOAT(mc_set_current, current, mc_set_current_calls, last_current)
FAKE_RECORD_FLOAT(mc_set_brake_current, current, mc_set_brake_current_calls, last_brake_current)
FAKE_RECORD_FLOAT(mc_set_duty, duty, mc_set_duty_calls, last_duty)
FAKE_RECORD_FLOAT(
    mc_set_current_off_delay, delay_sec, mc_set_current_off_delay_calls, last_current_off_delay
)

static bool fake_foc_play_tone(int channel, float freq, float voltage) {
    ++fake_state.foc_play_tone_calls;
    fake_state.last_foc_channel = channel;
    fake_state.last_foc_frequency = freq;
    fake_state.last_foc_voltage = voltage;
    return true;
}

static float fake_io_read_analog(VESC_PIN pin) {
    switch (pin) {
    case VESC_PIN_ADC1:
        return fake_state.adc1;
    case VESC_PIN_ADC2:
        return fake_state.adc2;
    default:
        return -1.0f;
    }
}

FAKE_STATIC_FLOAT_GETTER(get_ppm, ppm)
FAKE_STATIC_FLOAT_GETTER(get_ppm_age, ppm_age)

static remote_state fake_get_remote_state(void) {
    return fake_state.remote;
}

FAKE_STATIC_FLOAT_GETTER(imu_get_pitch, imu_pitch)
FAKE_STATIC_FLOAT_GETTER(imu_get_roll, imu_roll)
FAKE_STATIC_FLOAT_GETTER(imu_get_yaw, imu_yaw)

static void fake_imu_get_gyro(float *gyro) {
    gyro[0] = fake_state.imu_gyro[0];
    gyro[1] = fake_state.imu_gyro[1];
    gyro[2] = fake_state.imu_gyro[2];
}

static void fake_imu_get_quaternions(float *quat) {
    quat[0] = fake_state.imu_quat[0];
    quat[1] = fake_state.imu_quat[1];
    quat[2] = fake_state.imu_quat[2];
    quat[3] = fake_state.imu_quat[3];
}

static bool fake_should_terminate(void) {
    return fake_state.terminate_after_sleep && fake_state.sleep_us_calls > 0;
}

static bool fake_imu_startup_done(void) {
    return fake_state.imu_startup_done;
}

static uint32_t fake_timer_time_now(void) {
    return fake_state.timer_ticks;
}

static float fake_timer_seconds_elapsed_since(uint32_t time) {
    return (fake_state.timer_ticks - time) * 1e-6f;
}

FAKE_STATIC_GETTER(mc_fault_code, mc_get_fault, fault)

static const char *fake_mc_fault_to_string(mc_fault_code fault) {
    (void) fault;
    return "FAULT_CODE_ABS_OVER_CURRENT";
}
FAKE_STATIC_FLOAT_GETTER(mc_get_tot_current_in, motor_batt_current)
FAKE_STATIC_FLOAT_GETTER(mc_get_input_voltage_filtered, motor_voltage)

static float fake_mc_get_battery_level(float *wh_left) {
    if (wh_left) {
        *wh_left = 0.0f;
    }
    return fake_state.battery_level;
}

static volatile gnss_data *fake_mc_gnss(void) {
    ++fake_state.mc_gnss_calls;
    if (fake_state.gnss_returns_null) {
        return NULL;
    }
    return &fake_state.gnss;
}

FAKE_STATIC_FLOAT_GETTER(mc_get_distance_abs, motor_distance_abs)
FAKE_STATIC_RESET_FLOAT_GETTER(mc_get_amp_hours, amp_hours)
FAKE_STATIC_RESET_FLOAT_GETTER(mc_get_amp_hours_charged, amp_hours_charged)
FAKE_STATIC_RESET_FLOAT_GETTER(mc_get_watt_hours, watt_hours)
FAKE_STATIC_RESET_FLOAT_GETTER(mc_get_watt_hours_charged, watt_hours_charged)
FAKE_STATIC_GETTER(uint64_t, mc_get_odometer, odometer)

static bool fake_store_backup_data(void) {
    return true;
}

static float fake_foc_get_id(void) {
    return 0.0f;
}

static bool fake_read_eeprom_var(eeprom_var *v, int address) {
    ++fake_state.read_eeprom_var_calls;
    if (!fake_state.eeprom_read_enabled || address < 0 || address >= 512 ||
        fake_state.read_eeprom_var_calls == fake_state.eeprom_read_fail_call) {
        return false;
    }
    *v = fake_state.eeprom[address];
    return true;
}

static bool fake_store_eeprom_var(eeprom_var *v, int address) {
    ++fake_state.store_eeprom_var_calls;
    fake_state.last_eeprom_store_address = address;
    if (!fake_state.eeprom_store_enabled || address < 0 || address >= 512) {
        return false;
    }
    if (fake_state.store_eeprom_var_calls == fake_state.eeprom_store_fail_call) {
        return false;
    }
    if (fake_state.eeprom_store_discard ||
        fake_state.store_eeprom_var_calls == fake_state.eeprom_store_discard_call) {
        return true;
    }
    fake_state.eeprom[address] = *v;
    return true;
}

FAKE_STATIC_FLOAT_GETTER(mc_get_rpm, motor_rpm)
FAKE_STATIC_FLOAT_GETTER(mc_get_speed, motor_speed)
FAKE_STATIC_FLOAT_GETTER(mc_get_distance, motor_distance)
FAKE_STATIC_FLOAT_GETTER(mc_get_tot_current_filtered, motor_current)
FAKE_STATIC_FLOAT_GETTER(mc_get_tot_current_directional_filtered, motor_dir_current)
FAKE_STATIC_FLOAT_GETTER(mc_get_duty_cycle_now, motor_duty)
FAKE_STATIC_FLOAT_GETTER(mc_get_tot_current_in_filtered, motor_batt_current)
FAKE_STATIC_FLOAT_GETTER(mc_temp_fet_filtered, motor_fet_temp)
FAKE_STATIC_FLOAT_GETTER(mc_temp_motor_filtered, motor_temp)

static float fake_get_cfg_float(CFG_PARAM p) {
    return p <= CFG_PARAM_foc_motor_flux_linkage ? fake_state.cfg_float[p] : 0.0f;
}

static int fake_get_cfg_int(CFG_PARAM p) {
    return p <= CFG_PARAM_foc_motor_flux_linkage ? fake_state.cfg_int[p] : 0;
}

void vesc_if_fake_reset(void) {
    memset(&fake_vesc_if, 0, sizeof(fake_vesc_if));
    memset(fake_vesc_if_storage.package_data, 0, sizeof(fake_vesc_if_storage.package_data));
    reset_test_hw();
    memset(&fake_state, 0, sizeof(fake_state));
    fake_state.last_plot_graph = -1;
    fake_state.last_foc_channel = -1;
    fake_state.last_eeprom_store_address = -1;
    fake_state.imu_quat[0] = 1.0f;
    fake_state.fault = FAULT_CODE_NONE;

    fake_vesc_if.app_is_output_disabled = fake_app_is_output_disabled;
    fake_vesc_if.system_time = fake_system_time;
    fake_vesc_if.system_time_ticks = fake_system_time_ticks;
    fake_vesc_if.printf = fake_printf;
    fake_vesc_if.malloc = fake_malloc;
    fake_vesc_if.free = fake_free;
    fake_vesc_if.sleep_ms = fake_sleep_ms;
    fake_vesc_if.sleep_us = fake_sleep_us;
    fake_vesc_if.set_pad_mode = fake_set_pad_mode;
    fake_vesc_if.io_set_mode = fake_io_set_mode;
    fake_vesc_if.io_write = fake_io_write;
    fake_vesc_if.request_terminate = fake_request_terminate;
    fake_vesc_if.spawn = fake_spawn;
    fake_vesc_if.should_terminate = fake_should_terminate;
    fake_vesc_if.get_arg = fake_get_arg;
    fake_vesc_if.send_app_data = fake_send_app_data;
    fake_vesc_if.set_app_data_handler = fake_set_app_data_handler;
    fake_vesc_if.plot_init = fake_plot_init;
    fake_vesc_if.plot_add_graph = fake_plot_add_graph;
    fake_vesc_if.plot_set_graph = fake_plot_set_graph;
    fake_vesc_if.plot_send_points = fake_plot_send_points;
    fake_vesc_if.timeout_reset = fake_timeout_reset;
    fake_vesc_if.mc_set_current = fake_mc_set_current;
    fake_vesc_if.mc_set_brake_current = fake_mc_set_brake_current;
    fake_vesc_if.mc_set_duty = fake_mc_set_duty;
    fake_vesc_if.mc_set_current_off_delay = fake_mc_set_current_off_delay;
    fake_vesc_if.foc_play_tone = fake_foc_play_tone;
    fake_vesc_if.io_read_analog = fake_io_read_analog;
    fake_vesc_if.get_ppm = fake_get_ppm;
    fake_vesc_if.get_ppm_age = fake_get_ppm_age;
    fake_vesc_if.get_remote_state = fake_get_remote_state;
    fake_vesc_if.imu_get_pitch = fake_imu_get_pitch;
    fake_vesc_if.imu_get_roll = fake_imu_get_roll;
    fake_vesc_if.imu_get_yaw = fake_imu_get_yaw;
    fake_vesc_if.imu_get_gyro = fake_imu_get_gyro;
    fake_vesc_if.imu_get_quaternions = fake_imu_get_quaternions;
    fake_vesc_if.mc_get_fault = fake_mc_get_fault;
    fake_vesc_if.mc_fault_to_string = fake_mc_fault_to_string;
    fake_vesc_if.mc_get_rpm = fake_mc_get_rpm;
    fake_vesc_if.mc_get_speed = fake_mc_get_speed;
    fake_vesc_if.mc_get_distance = fake_mc_get_distance;
    fake_vesc_if.mc_get_tot_current_filtered = fake_mc_get_tot_current_filtered;
    fake_vesc_if.mc_get_tot_current_directional_filtered =
        fake_mc_get_tot_current_directional_filtered;
    fake_vesc_if.mc_get_duty_cycle_now = fake_mc_get_duty_cycle_now;
    fake_vesc_if.mc_get_tot_current_in = fake_mc_get_tot_current_in;
    fake_vesc_if.mc_get_tot_current_in_filtered = fake_mc_get_tot_current_in_filtered;
    fake_vesc_if.mc_get_input_voltage_filtered = fake_mc_get_input_voltage_filtered;
    fake_vesc_if.mc_temp_fet_filtered = fake_mc_temp_fet_filtered;
    fake_vesc_if.mc_temp_motor_filtered = fake_mc_temp_motor_filtered;
    fake_vesc_if.mc_get_battery_level = fake_mc_get_battery_level;
    fake_vesc_if.mc_get_distance_abs = fake_mc_get_distance_abs;
    fake_vesc_if.mc_get_amp_hours = fake_mc_get_amp_hours;
    fake_vesc_if.mc_get_amp_hours_charged = fake_mc_get_amp_hours_charged;
    fake_vesc_if.mc_get_watt_hours = fake_mc_get_watt_hours;
    fake_vesc_if.mc_get_watt_hours_charged = fake_mc_get_watt_hours_charged;
    fake_vesc_if.mc_get_odometer = fake_mc_get_odometer;
    fake_vesc_if.store_backup_data = fake_store_backup_data;
    fake_vesc_if.mc_gnss = fake_mc_gnss;
    fake_vesc_if.foc_get_id = fake_foc_get_id;
    fake_vesc_if.read_eeprom_var = fake_read_eeprom_var;
    fake_vesc_if.store_eeprom_var = fake_store_eeprom_var;
    fake_vesc_if.imu_startup_done = fake_imu_startup_done;
    fake_vesc_if.timer_time_now = fake_timer_time_now;
    fake_vesc_if.timer_seconds_elapsed_since = fake_timer_seconds_elapsed_since;
    fake_vesc_if.conf_custom_add_config = fake_conf_custom_add_config;
    fake_vesc_if.conf_custom_clear_configs = fake_conf_custom_clear_configs;
    fake_vesc_if.imu_set_read_callback = fake_imu_set_read_callback;
    fake_vesc_if.lbm_add_extension = fake_lbm_add_extension;
    fake_vesc_if.lbm_dec_as_float = fake_lbm_dec_as_float;
    fake_vesc_if.lbm_dec_as_i32 = fake_lbm_dec_as_i32;
    fake_vesc_if.lbm_enc_sym_nil = 0;
    fake_vesc_if.lbm_enc_sym_true = 1;
    fake_vesc_if.get_cfg_float = fake_get_cfg_float;
    fake_vesc_if.get_cfg_int = fake_get_cfg_int;
    fake_vesc_if.set_cfg_float = vesc_if_fake_set_cfg_float;
}

void vesc_if_fake_set_data_buffer(uint32_t magic, uint8_t *buffer, size_t length) {
    FakeDataBufferInfo info = {
        .magic = magic,
        .buffer = buffer,
        .length = length,
    };
    memcpy((uint8_t *) &fake_vesc_if + FAKE_VESC_PACKAGE_DATA_OFFSET, &info, sizeof(info));
}

FAKE_SETTER(set_ticks, systime_t, ticks, ticks)
FAKE_SETTER(set_seconds, float, seconds, seconds)
FAKE_SETTER(set_imu_startup_done, bool, done, imu_startup_done)
FAKE_SETTER(set_odometer, uint64_t, odometer, odometer)

void vesc_if_fake_set_terminate_after_sleep(bool enabled) {
    fake_state.terminate_after_sleep = enabled;
    if (enabled) {
        fake_state.sleep_us_calls = 0;
    }
}
FAKE_SETTER(set_output_disabled, bool, disabled, output_disabled)
FAKE_SETTER(set_eeprom_read_enabled, bool, enabled, eeprom_read_enabled)
FAKE_SETTER(set_eeprom_store_enabled, bool, enabled, eeprom_store_enabled)
FAKE_SETTER(set_eeprom_read_fail_call, size_t, call_number, eeprom_read_fail_call)
FAKE_SETTER(set_eeprom_store_fail_call, size_t, call_number, eeprom_store_fail_call)
FAKE_SETTER(set_eeprom_store_discard_call, size_t, call_number, eeprom_store_discard_call)
FAKE_SETTER(set_eeprom_store_discard, bool, discard, eeprom_store_discard)

uint32_t vesc_if_fake_eeprom_word(size_t address) {
    return address < 512u ? fake_state.eeprom[address].as_u32 : 0u;
}

void vesc_if_fake_fill_next_malloc(uint8_t value) {
    fake_state.malloc_fill_enabled = true;
    fake_state.malloc_fill_value = value;
}

void vesc_if_fake_fail_next_malloc(void) {
    fake_state.malloc_fail_enabled = true;
}

FAKE_SETTER(fail_malloc_call, size_t, call_number, malloc_fail_call)
FAKE_SETTER(fail_spawn_call, size_t, call_number, spawn_fail_call)

void vesc_if_fake_set_analog(float adc1, float adc2) {
    fake_state.adc1 = adc1;
    fake_state.adc2 = adc2;
}

void vesc_if_fake_set_ppm(float value, float age_s) {
    fake_state.ppm = value;
    fake_state.ppm_age = age_s;
}

void vesc_if_fake_set_remote(float js_y, float age_s) {
    fake_state.remote.js_y = js_y;
    fake_state.remote.age_s = age_s;
}

void vesc_if_fake_set_imu(
    float pitch, float roll, float yaw, float gyro_x, float gyro_y, float gyro_z
) {
    fake_state.imu_pitch = pitch;
    fake_state.imu_roll = roll;
    fake_state.imu_yaw = yaw;
    fake_state.imu_gyro[0] = gyro_x;
    fake_state.imu_gyro[1] = gyro_y;
    fake_state.imu_gyro[2] = gyro_z;
}

void vesc_if_fake_set_imu_quaternions(float q0, float q1, float q2, float q3) {
    fake_state.imu_quat[0] = q0;
    fake_state.imu_quat[1] = q1;
    fake_state.imu_quat[2] = q2;
    fake_state.imu_quat[3] = q3;
}

FAKE_SETTER(set_battery_level, float, level, battery_level)

void vesc_if_fake_set_gnss(
    double lat, double lon, float height, float speed, float hdop, systime_t last_update
) {
    fake_state.gnss.lat = lat;
    fake_state.gnss.lon = lon;
    fake_state.gnss.height = height;
    fake_state.gnss.speed = speed;
    fake_state.gnss.hdop = hdop;
    fake_state.gnss.last_update = last_update;
}

void vesc_if_fake_set_mc_gnss_missing(void) {
    fake_vesc_if.mc_gnss = NULL;
}

FAKE_SETTER(set_mc_gnss_returns_null, bool, returns_null, gnss_returns_null)
FAKE_SETTER(set_arg, void *, arg, prog_arg)

void vesc_if_fake_invoke_app_data_handler(unsigned char *data, unsigned int len) {
    fake_invoke_app_data_handler(data, len);
}

void vesc_if_fake_invoke_imu_callback(float *acc, float *gyro, float *mag, float dt) {
    fake_state.imu_read_callback(acc, gyro, mag, dt);
}

int vesc_if_fake_get_custom_config(uint8_t *data, bool is_default) {
    return fake_state.get_custom_cfg(data, is_default);
}

bool vesc_if_fake_set_custom_config(uint8_t *data) {
    return fake_state.set_custom_cfg(data);
}

int vesc_if_fake_get_custom_config_xml(uint8_t **data) {
    return fake_state.get_custom_cfg_xml(data);
}

lbm_value vesc_if_fake_invoke_extension(const char *name, lbm_value *args, lbm_uint argn) {
    extension_fptr extension =
        strcmp(name, "ext-bms") == 0 ? fake_state.ext_bms : fake_state.ext_set_fw_version;
    return extension(args, argn);
}

bool vesc_if_fake_set_cfg_float(CFG_PARAM param, float value) {
    if (param <= CFG_PARAM_foc_motor_flux_linkage) {
        fake_state.cfg_float[param] = value;
        return true;
    }
    return false;
}

void vesc_if_fake_set_cfg_int(CFG_PARAM param, int value) {
    if (param <= CFG_PARAM_foc_motor_flux_linkage) {
        fake_state.cfg_int[param] = value;
    }
}

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
) {
    fake_state.motor_rpm = rpm;
    fake_state.motor_speed = speed;
    fake_state.motor_distance = distance;
    fake_state.motor_distance_abs = distance;
    fake_state.motor_current = current;
    fake_state.motor_dir_current = dir_current;
    fake_state.motor_duty = duty;
    fake_state.motor_batt_current = batt_current;
    fake_state.motor_voltage = voltage;
    fake_state.motor_fet_temp = fet_temp;
    fake_state.motor_temp = motor_temp;
}

FAKE_SETTER(set_fault, mc_fault_code, fault, fault)

#define FAKE_COUNTER_ACCESSOR(field) FAKE_SIZE_GETTER(vesc_if_fake_##field, field)
FAKE_COUNTER_ACCESSOR(malloc_calls)
FAKE_COUNTER_ACCESSOR(free_calls)
FAKE_COUNTER_ACCESSOR(spawn_calls)
FAKE_COUNTER_ACCESSOR(request_terminate_calls)
FAKE_COUNTER_ACCESSOR(set_app_data_handler_calls)
FAKE_COUNTER_ACCESSOR(lbm_add_extension_calls)
FAKE_COUNTER_ACCESSOR(conf_custom_add_config_calls)
FAKE_COUNTER_ACCESSOR(conf_custom_clear_configs_calls)
FAKE_COUNTER_ACCESSOR(imu_set_read_callback_calls)
FAKE_COUNTER_ACCESSOR(set_pad_mode_calls)
FAKE_COUNTER_ACCESSOR(plot_init_calls)
FAKE_COUNTER_ACCESSOR(plot_add_graph_calls)
FAKE_COUNTER_ACCESSOR(plot_set_graph_calls)
FAKE_COUNTER_ACCESSOR(plot_send_points_calls)
FAKE_COUNTER_ACCESSOR(timeout_reset_calls)
FAKE_COUNTER_ACCESSOR(mc_set_current_calls)
FAKE_COUNTER_ACCESSOR(mc_set_brake_current_calls)
FAKE_COUNTER_ACCESSOR(mc_set_duty_calls)
FAKE_COUNTER_ACCESSOR(mc_set_current_off_delay_calls)
FAKE_COUNTER_ACCESSOR(foc_play_tone_calls)
FAKE_COUNTER_ACCESSOR(mc_gnss_calls)
FAKE_COUNTER_ACCESSOR(read_eeprom_var_calls)
FAKE_COUNTER_ACCESSOR(store_eeprom_var_calls)
FAKE_COUNTER_ACCESSOR(sleep_us_calls)
#undef FAKE_COUNTER_ACCESSOR

#define FAKE_VALUE_ACCESSOR(type, field) FAKE_GETTER(type, vesc_if_fake_##field, field)
FAKE_VALUE_ACCESSOR(void *, last_pad_gpio)
FAKE_VALUE_ACCESSOR(uint32_t, last_pad_pin)
FAKE_VALUE_ACCESSOR(uint32_t, last_pad_mode)
FAKE_VALUE_ACCESSOR(int, last_plot_graph)
FAKE_VALUE_ACCESSOR(float, last_plot_x)
FAKE_VALUE_ACCESSOR(float, last_plot_y)
FAKE_VALUE_ACCESSOR(float, last_current)
FAKE_VALUE_ACCESSOR(float, last_brake_current)
FAKE_VALUE_ACCESSOR(float, last_duty)
FAKE_VALUE_ACCESSOR(float, last_current_off_delay)
FAKE_VALUE_ACCESSOR(int, last_foc_channel)
FAKE_VALUE_ACCESSOR(float, last_foc_frequency)
FAKE_VALUE_ACCESSOR(float, last_foc_voltage)
FAKE_VALUE_ACCESSOR(int, last_eeprom_store_address)
#undef FAKE_VALUE_ACCESSOR

const uint8_t *vesc_if_fake_last_app_data(size_t *len) {
    if (len) {
        *len = fake_state.app_data_len;
    }
    return fake_state.app_data_len ? fake_state.app_data : NULL;
}
