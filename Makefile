# -----------------------------------------------------------------------------
# Gerätespezifikation
# -----------------------------------------------------------------------------
# DEVICE legt den exakten MCU‑Namen fest, den libopencm3 benötigt, um:
#   - das passende Linkerskript zu generieren
#   - die richtige Architektur (ARMv7E‑M / Cortex‑M4) zu setzen
#   - die passende libopencm3‑Library zu verlinken
#
# Der Name muss exakt in libopencm3/ld/devices.data vorkommen.
# Für den STM32F411CE (Blackpill) ist das "stm32f411ce".
DEVICE = stm32f411ce
# DEVICE = stm32f103C8
# DEVICE = stm32f429xx

# ARCH_FLAGS werden normalerweise automatisch durch genlink-config.mk gesetzt.
# Du überschreibst sie hier bewusst manuell, was völlig legitim ist.
ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 #411
# ARCH_FLAGS  = -mthumb -mcpu=cortex-m3 -msoft-float -mfix-cortex-m3-ldrd # 103
# ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 # 429

# DEFINES sind Makros, die an den Compiler übergeben werden.
# Sie steuern u.a. Header‑Auswahl, Peripherie‑Definitionen und Board‑Konfiguration.
DEFINES = -DSTM32F4 -DSTM32F411xE
# DEFINES = -DSTM32F1 -DSTM32F103x8
# DEFINES = -DSTM32F4 -DSTM32F429xx

# -----------------------------------------------------------------------------
# Pfad zur libopencm3‑Installation
# -----------------------------------------------------------------------------
# OPENCM3_DIR zeigt auf die Root‑Directory der libopencm3‑Quelle.
# Die mk/‑Module, die du gleich einbindest, befinden sich darin.
OPENCM3_DIR = libopencm3

# -----------------------------------------------------------------------------
# libopencm3 Buildsystem – Konfigurationsphase
# -----------------------------------------------------------------------------
# genlink-config.mk:
#   - setzt ARCH_FLAGS automatisch (falls nicht überschrieben)
#   - setzt CPPFLAGS (z.B. -DSTM32F4)
#   - setzt LDFLAGS (z.B. -Llibopencm3/lib)
#   - setzt LDLIBS (z.B. -lopencm3_stm32f4)
#   - setzt LDSCRIPT (z.B. stm32f411ce.ld)
#
# gcc-config.mk:
#   - definiert Compiler, Linker, Objcopy
#   - setzt Standard‑Flags
#   - bereitet die Regeln vor, die später durch gcc-rules.mk ausgeführt werden
include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

# -----------------------------------------------------------------------------
# Projektdateien
# -----------------------------------------------------------------------------
# SRC enthält alle C‑Dateien im src/‑Ordner.
# OBJS sind die zugehörigen Objektdateien.
SRC  = $(wildcard src/*.c)
OBJS = $(SRC:.c=.o)

# -----------------------------------------------------------------------------
# Compiler‑ und Linker‑Flags
# -----------------------------------------------------------------------------
# CFLAGS:
#   -Iinclude, -Isrc: eigene Header
#   -ffreestanding: keine Standardbibliothek
#   -fno-builtin: keine impliziten libc‑Funktionen
#   -nostdlib: keine automatische libc‑Einbindung
#   -O2: Optimierung
#   -Wall -Wextra: Warnungen
CFLAGS  += -Iinclude -Isrc -ffreestanding -fno-builtin -nostdlib -O2 -Wall -Wextra

# CPPFLAGS:
#   - enthält Makros wie -DSTM32F411xE
CPPFLAGS += $(DEFINES)

# LDFLAGS:
#   -nostartfiles: kein CRT0
#   -nostdlib: keine libc
#   (libopencm3 liefert alles, was du brauchst)
LDFLAGS += -nostartfiles -nostdlib

# -----------------------------------------------------------------------------
# Build‑Targets
# -----------------------------------------------------------------------------
# all:
#   Baut sowohl ELF als auch BIN.
all: firmware.elf firmware.bin

# clean:
#   Entfernt alle generierten Dateien.
clean:
	rm -f $(OBJS) firmware.elf firmware.bin

# -----------------------------------------------------------------------------
# libopencm3 Buildsystem – Regelphase
# -----------------------------------------------------------------------------
# genlink-rules.mk:
#   - erzeugt automatisch das Linkerskript $(DEVICE).ld
#   - ruft intern scripts/genlink.py auf
#
# gcc-rules.mk:
#   - kompiliert alle .c → .o
#   - linkt .o + LDSCRIPT → firmware.elf
#   - erzeugt firmware.bin
#
# Diese beiden Dateien enthalten die komplette Build‑Logik.
include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk

# -----------------------------------------------------------------------------
# Flash‑Target (OpenOCD)
# -----------------------------------------------------------------------------
# PROJECT:
#   Name der Firmware ohne Endung.
PROJECT = firmware

# OOCD:
#   OpenOCD‑Binary (überschreibbar)
OOCD            ?= openocd

# OOCD_INTERFACE:
#   Debug‑Probe (cmsis-dap, stlink, jlink)
OOCD_INTERFACE  ?= cmsis-dap

# OOCD_TARGET:
#   OpenOCD‑Target‑Definition für STM32F4‑Familie
OOCD_TARGET     ?= stm32f4x
# OOCD_TARGET     ?= stm32f1x

# flash:
#   Programmiert das ELF direkt (besser als BIN, da Sections erhalten bleiben).
flash: $(PROJECT).elf
	$(OOCD) -f interface/$(OOCD_INTERFACE).cfg \
	-f target/$(OOCD_TARGET).cfg \
	-c "program $< verify reset exit"

docs:
	doxygen Doxyfile

docs-clean:
	rm -rf docs/html docs/latex
