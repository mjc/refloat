// Copyright 2026 Michael Conrad
//
// This file is part of the Refloat VESC package.
//
// Refloat VESC package is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// Refloat VESC package is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.

#include "led_color_order.h"

static uint32_t color_grb(uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
    (void) w;
    return ((uint32_t) g << 16) | ((uint32_t) r << 8) | b;
}

static uint32_t color_grbw(uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) g << 24) | ((uint32_t) r << 16) | ((uint32_t) b << 8) | w;
}

static uint32_t color_rgb(uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
    (void) w;
    return ((uint32_t) r << 16) | ((uint32_t) g << 8) | b;
}

static uint32_t color_wrgb(uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) w << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | b;
}

bool led_color_order_resolve(LedColorOrder order, uint8_t *bits, LedColorConverter *converter) {
    switch (order) {
    case LED_COLOR_GRB:
        *bits = 24;
        *converter = color_grb;
        return true;
    case LED_COLOR_GRBW:
        *bits = 32;
        *converter = color_grbw;
        return true;
    case LED_COLOR_RGB:
        *bits = 24;
        *converter = color_rgb;
        return true;
    case LED_COLOR_WRGB:
        *bits = 32;
        *converter = color_wrgb;
        return true;
    }

    return false;
}
