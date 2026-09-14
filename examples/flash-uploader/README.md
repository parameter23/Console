# flash-uploader

One-shot tool firmware that streams a file from this machine (the one
running the debugger) into the onboard W25Q128 SPI flash, via ARM
semihosting file I/O over the existing SWD/OpenOCD link - no UART, no
extra wiring. Verified end-to-end on real hardware: erases the target
sectors, writes the file page by page, then reads both the file and the
flash back independently and compares them byte-for-byte before
reporting success.

Not a game - this only touches SPI1 and the flash, no display/GameAPI
involved.

## Why semihosting, not `make flash`

This board has no UART and no SD card. The USB-C port is wired to the
MCU's USB peripheral (so a proper CDC-ACM/MSC uploader is possible in
principle), but that needs a system clock change (100MHz -> 96MHz to
get an exact 48MHz USB clock) that would also detune the audio engine's
PWM/mixer timing - too invasive for what's meant to be an occasional
asset-upload tool. Semihosting reuses the CMSIS-DAP/OpenOCD link
already used for flashing every example, at the cost of being slower
(each file read is a synchronous round-trip over the debug link) - fine
for asset files in the tens/low hundreds of KB, not something you'd
want for megabytes.

## Usage

1. Build, targeting the file you want to upload and the flash address
   it should land at (must fit in 24 bits, i.e. < 16MB - `W25Q_TOTAL_SIZE`):

   ```bash
   make clean   # FILE/ADDR are compile-time - always clean first when changing them
   make FILE=path/to/image.bin ADDR=0x000000
   ```

   `FILE` is resolved on the HOST by OpenOCD at upload time (see step 2),
   not baked into the firmware image itself - only the *path string* is
   compiled in. Relative paths resolve against wherever `openocd` is
   run from.

2. Upload (NOT `make flash` - that flashes-and-detaches, and semihosting
   needs the debugger to stay attached and servicing the target's file
   requests):

   ```bash
   make upload
   ```

   This programs the firmware, enables semihosting, resumes the target,
   and then **stays in the foreground** printing the target's progress
   (`SYS_WRITE0` output) directly to this console:

   ```
   flash-uploader: opening 'path/to/image.bin'...
   file length: 25600 bytes
   erasing flash sectors...
   writing..........
   verifying... OK
   flash-uploader: done.
   ```

   Watch for `flash-uploader: done.` (LED on PC13 goes solid too), then
   `Ctrl-C` to stop OpenOCD - there's no automatic exit.

   On error (`ERROR: ...` printed, LED fast-blinks forever): the most
   likely cause is `FILE` not being found relative to wherever `openocd`
   was launched from - use an absolute path if in doubt.

## Multiple files / a real asset set

Repeat step 1+2 per file, once per `ADDR`. There's no on-flash directory
or table of contents - addresses are whatever the caller (this tool, and
whatever later reads the data back at runtime) agree on. See
`tools/README-img2fullscreen.md`'s `--format raw` section for how image
slot addresses are chosen (sector-aligned, one slot per input image in
argument order).

## Verifying independently

`w25q128-scan`/`w25q128-test` (sibling examples) can read the chip back
to confirm what's actually stored, if you want a second, completely
separate check beyond this tool's own built-in verify pass.
