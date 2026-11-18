# STM32H743 Multi-MCU Firmware Makefile

.DEFAULT_GOAL := firmware

TARGET   ?= mcu1
APP_SRC  ?= src/main_mcu1.c
RENODE   ?= 1
ifeq ($(RENODE),0)
BUILD_DIR = build/hardware/$(TARGET)
else
BUILD_DIR = build/$(TARGET)
endif

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU = -mcpu=cortex-m7 -mthumb
FPU = -mfpu=fpv5-d16 -mfloat-abi=hard

CFLAGS  = $(CPU) $(FPU)
CFLAGS += -std=gnu11 -Wall -Wextra
CFLAGS += -O0 -g3
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DSTM32H743xx -DNOCARC_RENODE=$(RENODE)
CFLAGS += -Iinclude -Iinclude/CMSIS -Isystem/inc

LDSCRIPT = linker/stm32h743.ld

LDFLAGS  = $(CPU) $(FPU)
LDFLAGS += --specs=nano.specs --specs=nosys.specs
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map
LDFLAGS += -Wl,--print-memory-usage

# Selected application + shared peripheral drivers
DRIVER_SOURCES = $(wildcard system/src/*.c)
# The two demos own their FDCAN1 vector and use their own controller handle.
ifneq ($(filter src/main_mcu1.c src/main_mcu2.c,$(APP_SRC)),)
DRIVER_SOURCES := $(filter-out system/src/FDCAN_Port.c system/src/peripherals_init.c,$(DRIVER_SOURCES))
endif
C_SOURCES = $(APP_SRC) $(APP_MODULES) $(DRIVER_SOURCES)

ASM_SOURCES = $(wildcard startup/*.s) $(wildcard startup/*.S)

C_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES))

ASM_OBJECTS = \
    $(patsubst startup/%.s,$(BUILD_DIR)/startup/%.o,$(filter %.s,$(ASM_SOURCES))) \
    $(patsubst startup/%.S,$(BUILD_DIR)/startup/%.o,$(filter %.S,$(ASM_SOURCES)))

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)
DEPFILES = $(C_OBJECTS:.o=.d)

ELF = $(BUILD_DIR)/$(TARGET).elf
BIN = $(BUILD_DIR)/$(TARGET).bin
HEX = $(BUILD_DIR)/$(TARGET).hex

all: $(ELF) $(BIN) $(HEX)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/startup/%.o: startup/%.s
	@mkdir -p $(dir $@)
	$(CC) $(CPU) -g3 -c $< -o $@

$(BUILD_DIR)/startup/%.o: startup/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -g3 -c $< -o $@

$(ELF): $(OBJECTS) $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

mcu1:
	$(MAKE) TARGET=mcu1 APP_SRC=src/main_mcu1.c all

mcu2:
	$(MAKE) TARGET=mcu2 APP_SRC=src/main_mcu2.c all

loopback:
	$(MAKE) TARGET=loopback APP_SRC=src/main.c all

driver_test:
	$(MAKE) TARGET=driver_test APP_SRC=src/main_driver_test.c all

clean:
	rm -rf build

.PHONY: all clean mcu1 mcu2 loopback driver_test

-include $(DEPFILES)

NODES = node1 node2 node3 node4
.PHONY: firmware $(NODES)
firmware: $(NODES)

$(NODES): node%:
	$(MAKE) TARGET=$@ APP_SRC=src/main_network.c \
		APP_MODULES="src/network_state.c src/telemetry.c" \
		CPPFLAGS="-DNODE_ID=$*" all

HOST_CC ?= cc
.PHONY: test sim-test
test:
	@mkdir -p build/tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Isrc tests/network_state_test.c src/network_state.c -o build/tests/network_state_test
	build/tests/network_state_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Isrc tests/network_protocol_test.c src/network_protocol.c -o build/tests/network_protocol_test
	build/tests/network_protocol_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Isrc tests/network_rx_ring_test.c -o build/tests/network_rx_ring_test
	build/tests/network_rx_ring_test
	PYTHONDONTWRITEBYTECODE=1 python3 tests/uart_protocol_test.py
	PYTHONDONTWRITEBYTECODE=1 python3 tests/uart_monitor_test.py

sim-test: firmware
	PYTHONDONTWRITEBYTECODE=1 python3 tests/network_sim.py
