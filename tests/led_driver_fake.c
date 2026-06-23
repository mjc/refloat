#include "led_driver.h"

#include <stdbool.h>
#include <stddef.h>

static size_t setup_calls;
static size_t paint_calls;
static size_t destroy_calls;
static bool setup_result = true;

void led_driver_fake_reset(void) {
    setup_calls = 0;
    paint_calls = 0;
    destroy_calls = 0;
    setup_result = true;
}

void led_driver_fake_set_setup_result(bool result) {
    setup_result = result;
}

size_t led_driver_fake_setup_calls(void) {
    return setup_calls;
}

size_t led_driver_fake_paint_calls(void) {
    return paint_calls;
}

size_t led_driver_fake_destroy_calls(void) {
    return destroy_calls;
}

void led_driver_init(LedDriver *driver) {
    driver->bitbuffer = NULL;
    driver->bitbuffer_length = 0;
    driver->pin = LED_PIN_B6;
    driver->pin_hw_config = NULL;
    for (size_t i = 0; i < STRIP_COUNT; ++i) {
        driver->strips[i] = NULL;
        driver->strip_bitbuffs[i] = NULL;
    }
}

bool led_driver_setup(
    LedDriver *driver, LedPin pin, LedPinConfig pin_config, const LedStrip **led_strips
) {
    (void) pin_config;
    ++setup_calls;
    driver->pin = pin;
    for (size_t i = 0; i < STRIP_COUNT; ++i) {
        driver->strips[i] = led_strips[i];
    }
    return setup_result;
}

void led_driver_paint(LedDriver *driver) {
    (void) driver;
    ++paint_calls;
}

void led_driver_destroy(LedDriver *driver) {
    (void) driver;
    ++destroy_calls;
}
