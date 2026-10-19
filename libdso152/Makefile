# FNIRSI DSO-152 Static Library Makefile

DEFAULT_TOOLCHAIN := /home/joao/projects/TCC-2/SDK/Lyra-sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux/bin/arm-none-eabi-
CROSS_COMPILE ?= $(if $(wildcard $(DEFAULT_TOOLCHAIN)gcc),$(DEFAULT_TOOLCHAIN),arm-none-eabi-)

CC      = $(CROSS_COMPILE)gcc
AR      = $(CROSS_COMPILE)ar
SIZE    = $(CROSS_COMPILE)size

MCU_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
CFLAGS  = $(MCU_FLAGS) -O2 -g -Wall -Wextra -ffunction-sections -fdata-sections
CFLAGS += -Iinclude -Icommon

SRCS = \
    src/dso152_hw.c \
    src/dso152_btn.c \
    src/dso152_lcd.c \
    src/dso152_afe.c \
    src/font8x8.c

BUILD_DIR = build
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET_LIB = libdso152.a

all: $(TARGET_LIB)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@echo "  CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_LIB): $(OBJS)
	@echo "  AR $@"
	@$(AR) rcs $@ $(OBJS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET_LIB)

.PHONY: all clean
