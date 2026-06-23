#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFRL;
    volatile uint32_t AFRH;
    volatile uint32_t BRR;
} stm32_gpio_t;

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
} TIM_TypeDef;

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t NDTR;
    volatile uint32_t PAR;
    volatile uint32_t M0AR;
    volatile uint32_t M1AR;
    volatile uint32_t FCR;
} DMA_Stream_TypeDef;

typedef struct {
    volatile uint32_t LISR;
    volatile uint32_t HISR;
    volatile uint32_t LIFCR;
    volatile uint32_t HIFCR;
} DMA_TypeDef;

typedef struct {
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB1ENR;
    volatile uint32_t AHB1ENR;
} RCC_TypeDef;

#define TEST_GPIOB_BASE 0x50000000u
#define TEST_GPIOC_BASE 0x50001000u
#define TEST_TIM3_BASE 0x50002000u
#define TEST_TIM4_BASE 0x50003000u
#define TEST_DMA1_STREAM0_BASE 0x50004000u
#define TEST_DMA1_STREAM2_BASE 0x50005000u
#define TEST_DMA1_STREAM3_BASE 0x50006000u
#define TEST_DMA1_BASE 0x50007000u
#define TEST_RCC_BASE 0x50008000u

#define GPIOB ((stm32_gpio_t *) TEST_GPIOB_BASE)
#define GPIOC ((stm32_gpio_t *) TEST_GPIOC_BASE)
#define TIM3 ((TIM_TypeDef *) TEST_TIM3_BASE)
#define TIM4 ((TIM_TypeDef *) TEST_TIM4_BASE)
#define DMA1_Stream0 ((DMA_Stream_TypeDef *) TEST_DMA1_STREAM0_BASE)
#define DMA1_Stream2 ((DMA_Stream_TypeDef *) TEST_DMA1_STREAM2_BASE)
#define DMA1_Stream3 ((DMA_Stream_TypeDef *) TEST_DMA1_STREAM3_BASE)
#define DMA1 ((DMA_TypeDef *) TEST_DMA1_BASE)
#define RCC ((RCC_TypeDef *) TEST_RCC_BASE)

#define PAL_STM32_MODE_ALTERNATE (2U << 0U)
#define PAL_STM32_OTYPE_PUSHPULL (0U << 2U)
#define PAL_STM32_OTYPE_OPENDRAIN (1U << 2U)
#define PAL_STM32_OSPEED_MID1 (1U << 3U)
#define PAL_STM32_ALTERNATE(n) ((uint32_t) (n) << 7U)
#define PAL_MODE_ALTERNATE(n) (PAL_STM32_MODE_ALTERNATE | PAL_STM32_ALTERNATE(n))

#define RCC_APB1Periph_TIM3 0x00000002u
#define RCC_APB1Periph_TIM4 0x00000004u
#define RCC_AHB1Periph_DMA1 0x00000020u

#define TIM_CounterMode_Up 0x0000u
#define TIM_OCMode_PWM1 0x0060u
#define TIM_OCPolarity_High 0x0000u
#define TIM_OutputState_Enable 0x0001u
#define TIM_OCPreload_Enable 0x0008u
#define TIM_CR1_ARPE 0x0080u
#define TIM_CR1_CEN 0x0001u
#define TIM_DMA_CC1 0x0200u
#define TIM_DMA_CC2 0x0400u
#define TIM_DMA_CC4 0x1000u

#define DMA_SxCR_EN 0x00000001u
#define DMA_Channel_2 0x04000000u
#define DMA_Channel_5 0x0A000000u
#define DMA_DIR_MemoryToPeripheral 0x00000040u
#define DMA_MemoryInc_Enable 0x00000400u
#define DMA_PeripheralDataSize_HalfWord 0x00000800u
#define DMA_MemoryDataSize_HalfWord 0x00002000u
#define DMA_Mode_Normal 0x00000000u
#define DMA_Priority_High 0x00020000u
#define DMA_MemoryBurst_Single 0x00000000u
#define DMA_PeripheralBurst_Single 0x00000000u
#define DMA_FIFOThreshold_Full 0x00000003u
#define DMA_LISR_FEIF0 0x00000001u
#define DMA_LISR_DMEIF0 0x00000004u
#define DMA_LISR_TEIF0 0x00000008u
#define DMA_LISR_HTIF0 0x00000010u
#define DMA_LISR_TCIF0 0x00000020u
#define DMA_LIFCR_CTCIF0 0x00000020u
