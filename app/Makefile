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
LIBS := -L$(DEVKITPRO)/portlibs/switch2/lib -L$(DEVKITPRO)/libounce/lib -lounce_core -lm

# devkitPro's maintained switch-mbedtls package is AArch64 and uses libnx
# compatible libc. It is optional for compiling old hardware diagnostics,
# but REQUIRED for real QR login (no insecure substitute crypto).
MBEDTLS_PREFIX := $(firstword $(foreach d,$(DEVKITPRO)/portlibs/switch2 $(DEVKITPRO)/portlibs/switch,$(if $(wildcard $(d)/lib/libmbedcrypto.a),$(d))))
ifneq ($(MBEDTLS_PREFIX),)
CFLAGS += -DDNX2_USE_MBEDTLS=1 -I$(MBEDTLS_PREFIX)/include
LIBS := -L$(MBEDTLS_PREFIX)/lib -lmbedcrypto $(LIBS)
else
$(warning switch-mbedtls not installed; NRO builds but QR login will report missing crypto library)
endif

all: $(TARGET).nro
$(BUILD):
	mkdir -p $@
$(BUILD)/main.o: source/main.c | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_packet.o: source/voice_packet.c source/voice_packet.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/discord_gateway_probe.o: source/discord_gateway_probe.c source/discord_gateway_probe.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/graphical_ui.o: source/graphical_ui.c source/graphical_ui.h source/brand_icon.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/discord_remote_probe.o: source/discord_remote_probe.c source/discord_gateway_probe.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/qrcodegen.o: source/vendor/qrcodegen.c source/vendor/qrcodegen.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/auth_crypto.o: source/auth_crypto.c source/auth_crypto.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/native_login.o: source/native_login.c source/native_login.h source/auth_crypto.h source/native_ws.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/native_font.o: source/native_font.c source/native_font.h source/pretendo_font.h source/vendor/stb_truetype.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/$(TARGET).elf: $(BUILD)/main.o $(BUILD)/voice_packet.o $(BUILD)/discord_gateway_probe.o $(BUILD)/graphical_ui.o $(BUILD)/discord_remote_probe.o $(BUILD)/qrcodegen.o $(BUILD)/auth_crypto.o $(BUILD)/native_login.o $(BUILD)/native_font.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
$(BUILD)/$(TARGET).nacp: | $(BUILD)
	$(DEVKITPRO)/tools/bin/nacptool --create "DiscordNX2" "Gamingwithgoose" "0.27" $@
$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp assets/icon.jpg
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --icon=assets/icon.jpg --nacp=$(BUILD)/$(TARGET).nacp
clean:
	rm -rf $(BUILD) $(TARGET).nro
.PHONY: all clean
