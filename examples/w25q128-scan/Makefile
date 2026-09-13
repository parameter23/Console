# -----------------------------------------------------------------------------
# w25q128-scan: auto-detects which SPI bus/CS pin an onboard W25Q128 flash is
# wired to (see main.c). Builds against the shared engine in ../../src and
# ../../include like any other example - `make` here produces
# firmware.elf/firmware.bin.
# -----------------------------------------------------------------------------
DEVICE = stm32f411ce
ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16

DEFINES = -DSTM32F4 -DSTM32F411xE

OPENCM3_DIR = ../../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

# Engine sources (shared with every example) + this example's own sources.
SRC  = $(wildcard ../../src/*.c) $(wildcard *.c)
OBJS = $(SRC:.c=.o)

CFLAGS  += -I../../include -I. -ffreestanding -fno-builtin -nostdlib -O2 -Wall -Wextra
CPPFLAGS += $(DEFINES)
LDFLAGS += -nostartfiles -nostdlib

all: firmware.elf firmware.bin

clean:
	rm -f $(OBJS) firmware.elf firmware.bin generated.$(DEVICE).ld

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk

PROJECT = firmware

OOCD            ?= openocd
OOCD_INTERFACE  ?= cmsis-dap
OOCD_TARGET     ?= stm32f4x

flash: $(PROJECT).elf
	$(OOCD) -f interface/$(OOCD_INTERFACE).cfg \
	-f target/$(OOCD_TARGET).cfg \
	-c "program $< verify reset exit"
