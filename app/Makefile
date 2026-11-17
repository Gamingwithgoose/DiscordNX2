# Adapted from ounce-devkit plugin-echo-client (MIT).
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO")
endif
TARGET := DiscordNX2
BUILD := build
PREFIX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
include $(DEVKITPRO)/libounce/switch2_cpu.mk
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CFLAGS := $(ARCH) -std=gnu11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -D__SWITCH__ -D__SWITCH2__ -I$(DEVKITPRO)/portlibs/switch2/include -I$(DEVKITPRO)/libounce/include
LDFLAGS := $(ARCH) -specs=$(DEVKITPRO)/libounce/switch.specs -Wl,--gc-sections
LIBS := -L$(DEVKITPRO)/portlibs/switch2/lib -L$(DEVKITPRO)/libounce/lib -lounce_core
all: $(TARGET).nro
$(BUILD):
	mkdir -p $@
$(BUILD)/main.o: source/main.c | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_packet.o: source/voice_packet.c source/voice_packet.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/discord_gateway_probe.o: source/discord_gateway_probe.c source/discord_gateway_probe.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/$(TARGET).elf: $(BUILD)/main.o $(BUILD)/voice_packet.o $(BUILD)/discord_gateway_probe.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
$(TARGET).nro: $(BUILD)/$(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2nro $< $@
clean:
	rm -rf $(BUILD) $(TARGET).nro
.PHONY: all clean
