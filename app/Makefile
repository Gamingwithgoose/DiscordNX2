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
# libkaihen is a linker script that includes the JIT hook object explicitly.
# libounce references these hooks weakly; its library alone cannot provide them.
LIBS := -L$(DEVKITPRO)/portlibs/switch2/lib -L$(DEVKITPRO)/libounce/lib -lkaihen -lounce_core -lm

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

VOICE_PREFIX := ../.local/voice/prefix
CFLAGS += -I$(VOICE_PREFIX)/include -Isource/vendor/monocypher -DDAVE_STATIC
LIBS := -L$(VOICE_PREFIX)/lib -Wl,--start-group -ldnx_dave -lopus -lcrypto -lstdc++ $(LIBS) -Wl,--end-group

all: $(TARGET).nro
$(BUILD)/voice_licenses.o: source/voice_licenses.S assets/voice-licenses.txt | $(BUILD)
	$(PREFIX)gcc $(ARCH) -c $< -o $@
$(BUILD)/voice_platform.o: source/voice_platform.c | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_engine.o: source/voice_engine.c source/voice_engine.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/monocypher.o: source/vendor/monocypher/monocypher.c source/vendor/monocypher/monocypher.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD):
	mkdir -p $@
$(BUILD)/main.o: source/main.c source/browser_memory.h source/wifi_browser_probe.h source/native_login.h source/session_store.h source/graphical_ui.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_packet.o: source/voice_packet.c source/voice_packet.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/discord_gateway_probe.o: source/discord_gateway_probe.c source/discord_gateway_probe.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/graphical_ui.o: source/graphical_ui.c source/graphical_ui.h source/brand_icon.h source/native_login.h source/session_store.h source/client_json.h source/chat_layout.h source/chat_assets.h source/mobile_keyboard.h source/chat_media_data.h source/client_data.h source/native_font.h source/message_presentation.h source/phone_text.h source/message_style.h source/profile_data.h source/unicode_text.h source/vendor/utf8proc/utf8proc.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/discord_remote_probe.o: source/discord_remote_probe.c source/discord_gateway_probe.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/qrcodegen.o: source/vendor/qrcodegen.c source/vendor/qrcodegen.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/auth_crypto.o: source/auth_crypto.c source/auth_crypto.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/native_login.o: source/native_login.c source/native_login.h source/auth_crypto.h source/native_ws.h source/captcha.h source/client_json.h source/client_data.h source/session_store.h source/live_gateway.inc source/chat_jobs.inc source/voice_connect.inc source/voice_controller.inc source/voice_runtime.inc source/voice_policy.h source/voice_members.inc source/voice_audio.inc source/voice_engine.h source/voice_control.h source/chat_media_data.h source/chat_assets.h source/message_presentation.h source/profile_data.h source/channel_memory.h source/gateway_session.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/utf8proc.o: source/vendor/utf8proc/utf8proc.c source/vendor/utf8proc/utf8proc.h source/vendor/utf8proc/utf8proc_data.c | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -DUTF8PROC_STATIC -c $< -o $@
$(BUILD)/chat_assets.o: source/chat_assets.c source/chat_assets.h source/vendor/stb_image.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/session_store.o: source/session_store.c source/session_store.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/unicode_fonts.o: source/unicode_fonts.S $(wildcard assets/fonts/*.ttf) | $(BUILD)
	$(PREFIX)gcc $(ARCH) -c $< -o $@
$(BUILD)/native_font.o: source/native_font.c source/native_font.h source/pretendo_font.h source/vendor/stb_truetype.h source/client_json.h source/unicode_text.h source/vendor/utf8proc/utf8proc.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
# Temporary static verification page, published from docs/ on GitHub Pages.
PHONE_CAPTCHA_ENABLED ?= 1
CFLAGS += -DDNX2_ENABLE_PHONE_HANDOFF=$(PHONE_CAPTCHA_ENABLED)
ifneq ($(strip $(PHONE_CAPTCHA_PAGE_URL)),)
CFLAGS += -DDNX2_PHONE_CAPTCHA_PAGE_URL='"$(PHONE_CAPTCHA_PAGE_URL)"'
endif
$(BUILD)/wifi_browser_probe.o: source/wifi_browser_probe.c source/wifi_browser_probe.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/browser_memory.o: source/browser_memory.c source/browser_memory.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/captcha.o: source/captcha.c source/captcha.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/captcha_phone.o: source/captcha_phone.c source/captcha.h source/captcha_phone_wire.h source/auth_crypto.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/captcha_phone_wire.o: source/captcha_phone_wire.c source/captcha_phone_wire.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/captcha_phone_crypto.o: source/captcha_phone_crypto.c source/captcha_phone_wire.h source/auth_crypto.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/$(TARGET).elf: $(BUILD)/voice_licenses.o $(BUILD)/voice_platform.o $(BUILD)/voice_engine.o $(BUILD)/monocypher.o $(BUILD)/utf8proc.o $(BUILD)/chat_assets.o $(BUILD)/unicode_fonts.o $(BUILD)/session_store.o $(BUILD)/wifi_browser_probe.o $(BUILD)/browser_memory.o $(BUILD)/captcha.o $(BUILD)/captcha_phone.o $(BUILD)/captcha_phone_wire.o $(BUILD)/captcha_phone_crypto.o $(BUILD)/main.o $(BUILD)/voice_packet.o $(BUILD)/discord_gateway_probe.o $(BUILD)/graphical_ui.o $(BUILD)/discord_remote_probe.o $(BUILD)/qrcodegen.o $(BUILD)/auth_crypto.o $(BUILD)/native_login.o $(BUILD)/native_font.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	$(DEVKITPRO)/tools/bin/nacptool --create "DiscordNX2" "Gamingwithgoose" "0.48" $@
$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp assets/icon.jpg
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --icon=assets/icon.jpg --nacp=$(BUILD)/$(TARGET).nacp
clean:
	rm -rf $(BUILD) $(TARGET).nro
.PHONY: all clean
