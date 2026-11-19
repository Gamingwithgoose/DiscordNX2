# Adapted from ounce-devkit plugin-echo (MIT).
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO")
endif
TARGET := $(if $(filter 1,$(VOICE)),discordnx2-voice,discordnx2)
BUILD := $(if $(filter 1,$(VOICE)),build-voice,build)
PREFIX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
include $(DEVKITPRO)/libounce/switch2_cpu.mk
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CFLAGS := $(ARCH) -std=gnu11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections -D__SWITCH__ -D__SWITCH2__ -I$(DEVKITPRO)/portlibs/switch2/include -I$(DEVKITPRO)/libounce/include
LDFLAGS := $(ARCH) -specs=$(DEVKITPRO)/libounce/switch.specs -Wl,--gc-sections
LIBS := -L$(DEVKITPRO)/portlibs/switch2/lib -L$(DEVKITPRO)/libounce/lib -lkaihen_plugin -lounce_core
all: $(TARGET).nso
$(BUILD):
	mkdir -p $@
$(BUILD)/main.o: source/main.c source/resident_service.inc ../app/source/resident_voice.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/$(TARGET).elf: $(BUILD)/main.o
	$(PREFIX)gcc $(LDFLAGS) $^ $(LIBS) -o $@
$(TARGET).nso: $(BUILD)/$(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2nso $< $@
clean:
	rm -rf $(BUILD) $(TARGET).nso
.PHONY: all clean

# Optional call-owning plugin; ordinary diagnostic build remains unchanged.
ifeq ($(VOICE),1)
BUILD := build-voice
CFLAGS += -DDNX2_RESIDENT_VOICE -DDNX2_RESIDENT_RUNTIME -DDNX2_USE_MBEDTLS=1 -DDAVE_STATIC -I../app/source -I../app/source/vendor/monocypher -I../.local/voice/prefix/include -I$(DEVKITPRO)/portlibs/switch/include
LIBS := -L../.local/voice/prefix/lib -L$(DEVKITPRO)/portlibs/switch/lib -Wl,--start-group -ldnx_dave -lopus -lcrypto -lstdc++ -lmbedcrypto $(LIBS) -lm -Wl,--end-group
VOICE_OBJECTS := $(BUILD)/native_login.o $(BUILD)/voice_engine.o $(BUILD)/voice_platform.o $(BUILD)/monocypher.o $(BUILD)/voice_licenses.o
$(BUILD)/native_login.o: ../app/source/native_login.c $(wildcard ../app/source/voice*.inc) ../app/source/resident_engine.inc ../app/source/resident_voice.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_engine.o: ../app/source/voice_engine.c ../app/source/voice_engine.h | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_platform.o: ../app/source/voice_platform.c | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/monocypher.o: ../app/source/vendor/monocypher/monocypher.c | $(BUILD)
	$(PREFIX)gcc $(CFLAGS) -c $< -o $@
$(BUILD)/voice_licenses.o: ../app/source/voice_licenses.S ../app/assets/voice-licenses.txt | $(BUILD)
	cd ../app && $(PREFIX)gcc $(ARCH) -c source/voice_licenses.S -o ../plugin/$@
$(BUILD)/$(TARGET).elf: $(VOICE_OBJECTS)
endif
