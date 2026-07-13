#pragma once

#include <stdint.h>

typedef struct {
    uint32_t dummy;
} TIM_TypeDef;

typedef struct {
    uint32_t dummy;
} DMA_Stream_TypeDef;

#define GPIOA_BASE 0x40020000u
#define GPIOB_BASE 0x40020400u
#define GPIOC_BASE 0x40020800u
#define GPIOD_BASE 0x40020C00u
#define GPIOE_BASE 0x40021000u
#define GPIOF_BASE 0x40021400u
#define GPIOG_BASE 0x40021800u
#define GPIOH_BASE 0x40021C00u
#define GPIOI_BASE 0x40022000u
