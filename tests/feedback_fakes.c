#include "leds.h"

typedef struct {
    size_t led_confirm_calls;
    uint16_t last_frequency;
    float last_intensity;
    float last_foc_frequency;
    float last_foc_voltage;
} FeedbackFakeState;

static FeedbackFakeState feedback_fake_state;

void feedback_fakes_reset(void) {
    feedback_fake_state = (FeedbackFakeState) {0};
}

size_t feedback_fakes_led_confirm_calls(void) {
    return feedback_fake_state.led_confirm_calls;
}

uint16_t feedback_fakes_last_frequency(void) {
    return feedback_fake_state.last_frequency;
}

float feedback_fakes_last_intensity(void) {
    return feedback_fake_state.last_intensity;
}

void leds_status_confirm(Leds *leds) {
    (void) leds;
    ++feedback_fake_state.led_confirm_calls;
}
