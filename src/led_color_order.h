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

#pragma once

#include "conf/datatypes.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t (*LedColorConverter)(uint8_t w, uint8_t r, uint8_t g, uint8_t b);

bool led_color_order_resolve(LedColorOrder order, uint8_t *bits, LedColorConverter *converter);
