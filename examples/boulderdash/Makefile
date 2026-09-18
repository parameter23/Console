# -----------------------------------------------------------------------------
# boulderdash: a port of a Windows Boulder Dash clone (github.com originally,
# see main.c's header comment) onto the reusable engine in ../../src and
# ../../include (see include/game.h for the GameAPI). Builds standalone -
# `make` here produces firmware.elf/firmware.bin for this example only.
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

# -I.          : this example's own headers
# -I../../include : the shared engine API (game.h, framebuffer8.h, ...)
# -DNDEBUG     : the ported game logic uses assert() in a few places; this
#                project has no libc under -nostdlib to back a real assert
#                failure path, so build it out like a normal release build.
CFLAGS  += -I../../include -I. -ffreestanding -fno-builtin -nostdlib -O2 -Wall -Wextra -DNDEBUG
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
