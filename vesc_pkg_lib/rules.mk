
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJDUMP = arm-none-eabi-objdump
OBJCOPY = arm-none-eabi-objcopy
PYTHON = python3

STLIB_PATH = $(VESC_C_LIB_PATH)/stdperiph_stm32f4/

ifeq ($(USE_STLIB),yes)
	SOURCES += \
		$(STLIB_PATH)/src/misc.c \
		$(STLIB_PATH)/src/stm32f4xx_adc.c \
		$(STLIB_PATH)/src/stm32f4xx_dma.c \
		$(STLIB_PATH)/src/stm32f4xx_exti.c \
		$(STLIB_PATH)/src/stm32f4xx_flash.c \
		$(STLIB_PATH)/src/stm32f4xx_rcc.c \
		$(STLIB_PATH)/src/stm32f4xx_syscfg.c \
		$(STLIB_PATH)/src/stm32f4xx_tim.c \
		$(STLIB_PATH)/src/stm32f4xx_iwdg.c \
		$(STLIB_PATH)/src/stm32f4xx_wwdg.c
endif

UTILS_PATH = $(VESC_C_LIB_PATH)/utils/

SOURCES += $(UTILS_PATH)/rb.c
SOURCES += $(UTILS_PATH)/utils.c

OBJECTS = $(SOURCES:.c=.so)
TARGET_ELF = $(TARGET).elf
TARGET_LIST = $(TARGET).list
TARGET_BIN = $(TARGET).bin
TARGET_LISP = $(TARGET).lisp

ifeq ($(USE_OPT),)
	USE_OPT =
endif

CFLAGS = -fpic -Os -Wall -Wextra -Wundef -std=gnu99 -I$(VESC_C_LIB_PATH)
CFLAGS += -I$(STLIB_PATH)/CMSIS/include -I$(STLIB_PATH)/CMSIS/ST -I$(STLIB_PATH)/inc -I$(UTILS_PATH)/
CFLAGS += -fomit-frame-pointer -falign-functions=16 -mthumb
CFLAGS += -fsingle-precision-constant -Wdouble-promotion
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mcpu=cortex-m4
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -DIS_VESC_LIB
CFLAGS += $(USE_OPT)

ifeq ($(USE_STLIB),yes)
	CFLAGS += -DUSE_STLIB
endif

LDFLAGS = -nostartfiles -static -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mcpu=cortex-m4
LDFLAGS += -lm -Wl,--gc-sections,--undefined=init
LDFLAGS += -T $(VESC_C_LIB_PATH)/link.ld

.PHONY: default all clean $(TARGET)

default: $(TARGET_LISP)
all: default

$(TARGET): $(TARGET_LISP)

%.so: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET_ELF) $(OBJECTS)

$(TARGET_ELF): $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET_LIST): $(TARGET_ELF)
	$(OBJDUMP) -D $< > $@

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@ --gap-fill 0x00

$(TARGET_LISP): $(TARGET_BIN)
	$(PYTHON) $(VESC_C_LIB_PATH)/conv.py -f $< -n $(TARGET) > $@

clean:
	rm -f $(OBJECTS) $(TARGET_ELF) $(TARGET_LIST) $(TARGET_LISP) $(TARGET_BIN) $(ADD_TO_CLEAN)
