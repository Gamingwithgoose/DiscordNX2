# SPDX-License-Identifier: MIT
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is required)
endif
TARGET := Browser-Research
PREFIX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
include $(DEVKITPRO)/libounce/switch2_cpu.mk
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CFLAGS := $(ARCH) -std=gnu11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -D__SWITCH__ -D__SWITCH2__ -I$(DEVKITPRO)/libounce/include -I$(DEVKITPRO)/portlibs/switch2/include
LDFLAGS := $(ARCH) -specs=$(DEVKITPRO)/libounce/switch.specs -Wl,--gc-sections
LIBS := -L$(DEVKITPRO)/portlibs/switch2/lib -L$(DEVKITPRO)/libounce/lib -lkaihen -lounce_core -lm
all: dist/$(TARGET).nro
build dist:
	mkdir -p $@
build/main.o: main.c copy_core.h request_policy.h | build
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
build/$(TARGET).elf: build/main.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
build/$(TARGET).nacp: Makefile | build
	$(DEVKITPRO)/tools/bin/nacptool --create "Browser Research" "Gamingwithgoose" "0.2" $@
dist/$(TARGET).nro: build/$(TARGET).elf build/$(TARGET).nacp | dist
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --nacp=build/$(TARGET).nacp
.PHONY: all
