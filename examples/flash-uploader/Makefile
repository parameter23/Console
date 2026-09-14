# -----------------------------------------------------------------------------
# flash-uploader: one-shot tool firmware that streams a host file into the
# onboard W25Q128 SPI flash via ARM semihosting (see README.md for the full
# workflow - this is NOT run with the usual `make flash`). Builds against the
# shared engine in ../../src and ../../include like any other example.
# -----------------------------------------------------------------------------
DEVICE = stm32f411ce
ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16

DEFINES = -DSTM32F4 -DSTM32F411xE

# Which host file to upload and where on the flash it lands. Compile-time
# on purpose (see main.c) - rebuild (a plain `make` won't notice a changed
# FILE/ADDR without `make clean` first, since flag changes aren't tracked
# as a dependency here) when either changes.
FILE ?= asset.bin
ADDR ?= 0x000000

OPENCM3_DIR = ../../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

# Engine sources (shared with every example) + this example's own sources.
SRC  = $(wildcard ../../src/*.c) $(wildcard *.c)
OBJS = $(SRC:.c=.o)

CFLAGS  += -I../../include -I. -ffreestanding -fno-builtin -nostdlib -O2 -Wall -Wextra
CPPFLAGS += $(DEFINES) -DUPLOAD_FILENAME='"$(FILE)"' -DUPLOAD_FLASH_ADDR=$(ADDR)u
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

# Plain flash+run, same as every other example - but this firmware will
# just fault on its first semihosting call with no debugger attached to
# service it. Use `make upload` instead for an actual upload.
flash: $(PROJECT).elf
	$(OOCD) -f interface/$(OOCD_INTERFACE).cfg \
	-f target/$(OOCD_TARGET).cfg \
	-c "program $< verify reset exit"

# Programs the firmware, enables semihosting, and resumes - then STAYS
# ATTACHED (foreground) so OpenOCD can service the target's file-read
# requests and print its SYS_WRITE0 progress/status to this console. Watch
# for "flash-uploader: done." (or the fast LED blink on error), then Ctrl-C
# to stop OpenOCD once it's done - there's no automatic exit.
upload: $(PROJECT).elf
	$(OOCD) -f interface/$(OOCD_INTERFACE).cfg \
	-f target/$(OOCD_TARGET).cfg \
	-c "program $< verify" \
	-c "reset init" \
	-c "arm semihosting enable" \
	-c "resume"
