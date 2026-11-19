# SPDX-License-Identifier: MIT
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is required)
endif
TARGET := Hardware-Tester
PREFIX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
include $(DEVKITPRO)/libounce/switch2_cpu.mk
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CFLAGS := $(ARCH) -std=gnu11 -O2 -Wall -Wextra -D__SWITCH__ -D__SWITCH2__ -I$(DEVKITPRO)/libounce/include
LDFLAGS := $(ARCH) -specs=$(DEVKITPRO)/libounce/switch.specs -Wl,--gc-sections
LIBS := -L$(DEVKITPRO)/libounce/lib -lounce_core -lm
all: dist/$(TARGET).nro
build:
	mkdir -p build
dist:
	mkdir -p dist
build/main.o: main.c | build
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
build/$(TARGET).elf: build/main.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
build/$(TARGET).nacp: Makefile | build
	$(DEVKITPRO)/tools/bin/nacptool --create "NS2 Hardware Tester" "Gamingwithgoose" "0.1" $@
dist/$(TARGET).nro: build/$(TARGET).elf build/$(TARGET).nacp | dist
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --nacp=build/$(TARGET).nacp
clean:
	rm -rf build dist
.PHONY: all clean
