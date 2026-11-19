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

# Focused startup diagnostic. Kept separate from the content exporter.
startup-check: dist/Browser-Startup-Check.nro
build/startup_check.o: startup_check.c | build
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
build/Browser-Startup-Check.elf: build/startup_check.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
build/Browser-Startup-Check.nacp: Makefile | build
	$(DEVKITPRO)/tools/bin/nacptool --create "Browser Startup Check" "Gamingwithgoose" "0.1" $@
dist/Browser-Startup-Check.nro: build/Browser-Startup-Check.elf build/Browser-Startup-Check.nacp | dist
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --nacp=build/Browser-Startup-Check.nacp
.PHONY: startup-check
