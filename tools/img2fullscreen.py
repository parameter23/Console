#!/usr/bin/env python3
"""
img2fullscreen.py
Convert one or more images into full-screen (320x240) C sprite16_t data
for this engine, e.g.:

const sprite16_t moor_image[FULLSCREEN_TILES] = {
    { .px = { {6,6,6,...}, ... } },   /* tile 0: row 0, col 0 */
    ...
};

Unlike tools/png2tileset.py (which slices a *tileable* illustration into
a deduplicated tileset + index grid - the right shape for pixel art with
repeated tiles), this is for one continuous painted/photographic image
meant to fill the whole 320x240 screen: no dedup (a painting has almost
no two identical 16x16 tiles anyway), and the source image is
auto-cropped/resized to fit, since these things rarely arrive already
320x240. Dithering defaults to ON here (off in png2tileset.py) because
that is what continuous-tone art actually looks like on 16 colors -
plain quantizing a photo/painting looks much rougher than a dithered
one, the opposite tradeoff from flat-color pixel art.

IMPORTANT - flash budget: a 320x240 image is 320*240 = 76800 bytes (one
byte per pixel, no compression) - about 75 KB. The whole chip has 512 KB
of flash for everything (engine + game code + all image/tile/sprite
data). This script prints each image's size and a running total against
that budget so you can see immediately whether what you're generating
will actually fit once you =#include= it in a build.

Requires:
    python3 -m pip install pillow

Usage:
    ./img2fullscreen.py                       # convert every image in cwd
    ./img2fullscreen.py Moor.jpg Markt.jpg -o out/
    ./img2fullscreen.py *.jpg --no-dither
    ./img2fullscreen.py Moor.jpg --fit contain --width 320 --height 80
"""

import argparse
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Fehler: Das Paket 'pillow' fehlt.", file=sys.stderr)
    print("Installieren mit: python3 -m pip install pillow", file=sys.stderr)
    sys.exit(1)

TILE = 16
IMAGE_EXTS = (".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp")
FLASH_BUDGET_BYTES = 512 * 1024

# W25Q128_SECTOR_SIZE, kept in sync by hand with W25Q_SECTOR_SIZE in
# include/w25q128.h - raw-mode slot addresses are rounded up to this so
# each image starts on an eraseable sector boundary.
W25Q_SECTOR_SIZE = 4096

# The engine's full 256-color palette (src/framebuffer8.c): 16 named
# colors (base_palette[]) + a 6x6x6 RGB cube and 24-step grayscale ramp
# (ext_palette[]), as RGB565. Kept in sync by hand; pass --palette-from
# to read it straight out of a framebuffer8.c instead of trusting this
# copy.
DEFAULT_PALETTE_RGB565 = [
    0x0000, 0xFFFF, 0x8800, 0x0639, 0xC897, 0x04A8, 0x0015, 0xEEE7,
    0xDD86, 0x6222, 0xFB2C, 0x4228, 0x8C51, 0x8FF1, 0x777F, 0xBDD7,
    0x0000, 0x0006, 0x000C, 0x0013, 0x0019, 0x001F, 0x01A0, 0x01A6,
    0x01AC, 0x01B3, 0x01B9, 0x01BF, 0x0320, 0x0326, 0x032C, 0x0333,
    0x0339, 0x033F, 0x04C0, 0x04C6, 0x04CC, 0x04D3, 0x04D9, 0x04DF,
    0x0640, 0x0646, 0x064C, 0x0653, 0x0659, 0x065F, 0x07E0, 0x07E6,
    0x07EC, 0x07F3, 0x07F9, 0x07FF, 0x3000, 0x3006, 0x300C, 0x3013,
    0x3019, 0x301F, 0x31A0, 0x31A6, 0x31AC, 0x31B3, 0x31B9, 0x31BF,
    0x3320, 0x3326, 0x332C, 0x3333, 0x3339, 0x333F, 0x34C0, 0x34C6,
    0x34CC, 0x34D3, 0x34D9, 0x34DF, 0x3640, 0x3646, 0x364C, 0x3653,
    0x3659, 0x365F, 0x37E0, 0x37E6, 0x37EC, 0x37F3, 0x37F9, 0x37FF,
    0x6000, 0x6006, 0x600C, 0x6013, 0x6019, 0x601F, 0x61A0, 0x61A6,
    0x61AC, 0x61B3, 0x61B9, 0x61BF, 0x6320, 0x6326, 0x632C, 0x6333,
    0x6339, 0x633F, 0x64C0, 0x64C6, 0x64CC, 0x64D3, 0x64D9, 0x64DF,
    0x6640, 0x6646, 0x664C, 0x6653, 0x6659, 0x665F, 0x67E0, 0x67E6,
    0x67EC, 0x67F3, 0x67F9, 0x67FF, 0x9800, 0x9806, 0x980C, 0x9813,
    0x9819, 0x981F, 0x99A0, 0x99A6, 0x99AC, 0x99B3, 0x99B9, 0x99BF,
    0x9B20, 0x9B26, 0x9B2C, 0x9B33, 0x9B39, 0x9B3F, 0x9CC0, 0x9CC6,
    0x9CCC, 0x9CD3, 0x9CD9, 0x9CDF, 0x9E40, 0x9E46, 0x9E4C, 0x9E53,
    0x9E59, 0x9E5F, 0x9FE0, 0x9FE6, 0x9FEC, 0x9FF3, 0x9FF9, 0x9FFF,
    0xC800, 0xC806, 0xC80C, 0xC813, 0xC819, 0xC81F, 0xC9A0, 0xC9A6,
    0xC9AC, 0xC9B3, 0xC9B9, 0xC9BF, 0xCB20, 0xCB26, 0xCB2C, 0xCB33,
    0xCB39, 0xCB3F, 0xCCC0, 0xCCC6, 0xCCCC, 0xCCD3, 0xCCD9, 0xCCDF,
    0xCE40, 0xCE46, 0xCE4C, 0xCE53, 0xCE59, 0xCE5F, 0xCFE0, 0xCFE6,
    0xCFEC, 0xCFF3, 0xCFF9, 0xCFFF, 0xF800, 0xF806, 0xF80C, 0xF813,
    0xF819, 0xF81F, 0xF9A0, 0xF9A6, 0xF9AC, 0xF9B3, 0xF9B9, 0xF9BF,
    0xFB20, 0xFB26, 0xFB2C, 0xFB33, 0xFB39, 0xFB3F, 0xFCC0, 0xFCC6,
    0xFCCC, 0xFCD3, 0xFCD9, 0xFCDF, 0xFE40, 0xFE46, 0xFE4C, 0xFE53,
    0xFE59, 0xFE5F, 0xFFE0, 0xFFE6, 0xFFEC, 0xFFF3, 0xFFF9, 0xFFFF,
    0x0000, 0x0861, 0x18A3, 0x2104, 0x2965, 0x39C7, 0x4228, 0x4A69,
    0x5ACB, 0x632C, 0x6B6D, 0x7BCF, 0x8430, 0x9492, 0x9CD3, 0xA534,
    0xB596, 0xBDD7, 0xC638, 0xD69A, 0xDEFB, 0xE75C, 0xF79E, 0xFFFF,
]


def rgb565_to_rgb888(v: int) -> tuple[int, int, int]:
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def _extract_array(text: str, name: str, size: int, path: Path) -> list[int]:
    m = re.search(rf"{name}\[{size}\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not m:
        print(f"Fehler: Kein '{name}[{size}] = {{...}}' in {path} gefunden.",
              file=sys.stderr)
        sys.exit(1)
    values = re.findall(r"0x[0-9A-Fa-f]{1,4}", m.group(1))
    if len(values) != size:
        print(f"Fehler: {len(values)} Werte statt {size} in {name}[] "
              f"gefunden ({path}).", file=sys.stderr)
        sys.exit(1)
    return [int(v, 16) for v in values]


def load_palette_from_source(path: Path) -> list[int]:
    """Pulls the full 256-color palette (base_palette[16] + ext_palette[240])
    straight out of a framebuffer8.c, in array order, so this tool can't
    drift out of sync with the engine's actual palette."""
    text = path.read_text(encoding="utf-8")
    return (_extract_array(text, "base_palette", 16, path)
            + _extract_array(text, "ext_palette", 240, path))


def nearest_index(rgb: tuple[int, int, int], palette_rgb: list[tuple[int, int, int]]) -> int:
    r, g, b = rgb
    best_i, best_d = 0, None
    for i, (pr, pg, pb) in enumerate(palette_rgb):
        d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if best_d is None or d < best_d:
            best_i, best_d = i, d
    return best_i


def fit_to_size(img: Image.Image, w: int, h: int, mode: str) -> Image.Image:
    """Resizes/crops a source image to exactly (w, h)."""
    sw, sh = img.size
    target_ratio = w / h
    src_ratio = sw / sh

    if mode == "stretch":
        return img.resize((w, h), Image.LANCZOS)

    if mode == "contain":
        scale = min(w / sw, h / sh)
        nw, nh = round(sw * scale), round(sh * scale)
        resized = img.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new("RGB", (w, h), (0, 0, 0))
        canvas.paste(resized, ((w - nw) // 2, (h - nh) // 2))
        return canvas

    # mode == "cover" (default): crop to the target aspect ratio first,
    # then resize down - preserves as much of the frame as possible
    # without distorting proportions or leaving black bars.
    if src_ratio > target_ratio:
        # source is relatively wider than target -> crop left/right
        new_sw = round(sh * target_ratio)
        x0 = (sw - new_sw) // 2
        img = img.crop((x0, 0, x0 + new_sw, sh))
    else:
        # source is relatively taller than target -> crop top/bottom
        new_sh = round(sw / target_ratio)
        y0 = (sh - new_sh) // 2
        img = img.crop((0, y0, sw, y0 + new_sh))
    return img.resize((w, h), Image.LANCZOS)


def quantize(img: Image.Image, palette_rgb: list[tuple[int, int, int]], dither: bool) -> list[list[int]]:
    """Returns a [h][w] grid of palette indices."""
    w, h = img.size
    px = img.load()
    work = [[list(px[x, y]) for x in range(w)] for y in range(h)]
    out = [[0] * w for _ in range(h)]

    for y in range(h):
        for x in range(w):
            r, g, b = work[y][x]
            r = min(255, max(0, round(r)))
            g = min(255, max(0, round(g)))
            b = min(255, max(0, round(b)))
            idx = nearest_index((r, g, b), palette_rgb)
            out[y][x] = idx

            if not dither:
                continue

            pr, pg, pb = palette_rgb[idx]
            er, eg, eb = r - pr, g - pg, b - pb

            def spread(nx, ny, frac):
                if 0 <= nx < w and 0 <= ny < h:
                    work[ny][nx][0] += er * frac
                    work[ny][nx][1] += eg * frac
                    work[ny][nx][2] += eb * frac

            spread(x + 1, y,     7 / 16)
            spread(x - 1, y + 1, 3 / 16)
            spread(x,     y + 1, 5 / 16)
            spread(x + 1, y + 1, 1 / 16)

    return out


def slice_tiles(index_grid: list[list[int]]) -> list[tuple[int, ...]]:
    """Cuts a [h][w] index grid into row-major 16x16 tiles - no dedup,
    unlike png2tileset.py: a painted/photographic image has almost no
    two identical tiles, so a tileset+grid indirection would just add a
    lookup for no size benefit. One flat array, same order as the map."""
    h = len(index_grid)
    w = len(index_grid[0])
    rows, cols = h // TILE, w // TILE
    tiles = []
    for ty in range(rows):
        for tx in range(cols):
            flat = []
            for sy in range(TILE):
                row = index_grid[ty * TILE + sy]
                flat.extend(row[tx * TILE:tx * TILE + TILE])
            tiles.append(tuple(flat))
    return tiles


def c_identifier(name: str) -> str:
    ident = re.sub(r"\W", "_", name)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    return ident.lower() or "image"


def emit_c(tiles: list[tuple[int, ...]], array_name: str, source_name: str) -> str:
    lines = [
        "/**",
        " * @file (generated)",
        f" * @brief Auto-generated by tools/img2fullscreen.py from",
        f" *        '{source_name}' - do not hand-edit, re-run the script instead.",
        " */",
        '#include "sprite16.h"',
        "",
        f"const sprite16_t {array_name}[{len(tiles)}] = {{",
    ]
    for i, tile in enumerate(tiles):
        lines.append(f"    /* tile {i} */")
        lines.append("    { .px = {")
        for r in range(TILE):
            row = tile[r * TILE:(r + 1) * TILE]
            lines.append("        {" + ",".join(str(v) for v in row) + "},")
        lines.append("    } },")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def emit_raw(index_grid: list[list[int]]) -> bytes:
    """Flattens a [h][w] index grid into a row-major, 1-byte/pixel blob -
    the exact same layout as framebuffer8[] (see include/framebuffer8.h),
    so this can be w25q_read() straight into the framebuffer with no
    unpacking. Meant for the external W25Q128 flash, not for compiling
    into firmware - see tools/README-img2fullscreen.md."""
    return bytes(v for row in index_grid for v in row)


def emit_h(array_name: str, rows: int, cols: int, guard: str) -> str:
    return "\n".join([
        "/**",
        " * @file (generated)",
        " * @brief Auto-generated by tools/img2fullscreen.py - do not hand-edit.",
        " */",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "sprite16.h"',
        "",
        f"#define {array_name.upper()}_COLS {cols}",
        f"#define {array_name.upper()}_ROWS {rows}",
        f"#define {array_name.upper()}_TILES ({cols} * {rows})",
        "",
        f"extern const sprite16_t {array_name}[{array_name.upper()}_TILES];",
        "",
        "#endif",
        "",
    ])


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="*", help="Source images; default: every "
                     f"{'/'.join(IMAGE_EXTS)} file in the current directory")
    ap.add_argument("-o", "--outdir", default=".", type=Path,
                     help="Output directory for the .c/.h pairs (default: .)")
    ap.add_argument("--width", type=int, default=320, help="Target width in px (default: 320, must be a multiple of 16)")
    ap.add_argument("--height", type=int, default=240, help="Target height in px (default: 240, must be a multiple of 16)")
    ap.add_argument("--fit", choices=["cover", "contain", "stretch"], default="cover",
                     help="cover: crop to fill the screen (default). "
                          "contain: fit the whole image, black bars if needed. "
                          "stretch: fill exactly, may distort proportions.")
    ap.add_argument("--palette-from", type=Path, help="Read the 16-color "
                     "palette straight out of this framebuffer8.c instead "
                     "of the copy built into this script")
    ap.add_argument("--no-dither", action="store_true", help="Plain nearest-"
                     "color quantizing instead of Floyd-Steinberg dithering "
                     "(dithering is the default here - it suits continuous-"
                     "tone art; use --no-dither for already-flat pixel art)")
    ap.add_argument("--name-prefix", default="", help="Prefix for the generated C identifiers")
    ap.add_argument("--name-suffix", default="_image", help="Suffix for the generated C identifiers (default: _image)")
    ap.add_argument("--format", choices=["c", "raw"], default="c",
                     help="c (default): compiled-in sprite16_t array (.c/.h), "
                          "for internal STM32 flash. raw: flat 1-byte/pixel "
                          ".bin (framebuffer8 layout, no tiling/C wrapper), "
                          "for the external W25Q128 flash - see "
                          "tools/README-img2fullscreen.md.")
    args = ap.parse_args()

    if args.width % TILE or args.height % TILE:
        print(f"Fehler: --width/--height muessen Vielfache von {TILE} sein "
              f"(got {args.width}x{args.height}).", file=sys.stderr)
        sys.exit(1)

    inputs = [Path(p) for p in args.inputs]
    if not inputs:
        inputs = sorted(p for p in Path(".").iterdir() if p.suffix.lower() in IMAGE_EXTS)
    if not inputs:
        print("Keine Bilder gefunden (und keine als Argument angegeben).", file=sys.stderr)
        sys.exit(1)

    palette565 = (load_palette_from_source(args.palette_from)
                  if args.palette_from else DEFAULT_PALETTE_RGB565)
    palette_rgb = [rgb565_to_rgb888(v) for v in palette565]

    args.outdir.mkdir(parents=True, exist_ok=True)

    rows, cols = args.height // TILE, args.width // TILE
    per_image_bytes = rows * cols * TILE * TILE
    total_bytes = 0

    if args.format == "raw":
        slot_bytes = -(-per_image_bytes // W25Q_SECTOR_SIZE) * W25Q_SECTOR_SIZE
        print(f"Ziel: {args.width}x{args.height}, {per_image_bytes} Bytes "
              f"({per_image_bytes/1024:.1f} KB) pro Bild, Flash-Slot "
              f"{slot_bytes} Bytes (naechste {W25Q_SECTOR_SIZE}-Byte-"
              f"Sektorgrenze)\n")
    else:
        print(f"Ziel: {args.width}x{args.height} ({cols}x{rows} Kacheln), "
              f"{per_image_bytes} Bytes ({per_image_bytes/1024:.1f} KB) pro Bild\n")

    for slot, in_path in enumerate(inputs):
        img = Image.open(in_path).convert("RGB")
        img = fit_to_size(img, args.width, args.height, args.fit)
        index_grid = quantize(img, palette_rgb, dither=not args.no_dither)

        if args.format == "raw":
            array_name = c_identifier(args.name_prefix + in_path.stem)
            out_bin = args.outdir / f"{array_name}.bin"
            out_bin.write_bytes(emit_raw(index_grid))

            slot_addr = slot * slot_bytes
            print(f"{in_path.name}: {img.size[0]}x{img.size[1]} -> {out_bin.name} "
                  f"({per_image_bytes/1024:.1f} KB) - Slot {slot} @ Flash-Adresse "
                  f"0x{slot_addr:06X}", flush=True)
            continue

        tiles = slice_tiles(index_grid)

        array_name = c_identifier(args.name_prefix + in_path.stem + args.name_suffix)
        out_c = args.outdir / f"{array_name}.c"
        out_h = args.outdir / f"{array_name}.h"

        out_c.write_text(emit_c(tiles, array_name, in_path.name), encoding="utf-8")
        guard = c_identifier(array_name).upper() + "_H"
        out_h.write_text(emit_h(array_name, rows, cols, guard), encoding="utf-8")

        total_bytes += per_image_bytes
        pct = total_bytes / FLASH_BUDGET_BYTES * 100
        print(f"{in_path.name}: {img.size[0]}x{img.size[1]} -> {out_c.name} "
              f"({per_image_bytes/1024:.1f} KB) - running total "
              f"{total_bytes/1024:.1f} KB / {FLASH_BUDGET_BYTES/1024:.0f} KB flash ({pct:.0f}%)",
              flush=True)

    if args.format == "c" and total_bytes > FLASH_BUDGET_BYTES:
        print(f"\nWARNUNG: {total_bytes/1024:.1f} KB Bilddaten allein "
              f"uebersteigen bereits die {FLASH_BUDGET_BYTES/1024:.0f} KB "
              f"Flash des STM32F411 - das passt so nicht neben Engine- und "
              f"Spielcode in ein einzelnes Firmware-Image. Nur eine Auswahl "
              f"der Bilder gleichzeitig einbinden, die Aufloesung senken "
              f"(--width/--height) oder die Bilder als Kachel-Illustration "
              f"statt Vollbild nutzen (siehe png2tileset.py), oder "
              f"--format raw fuer den externen W25Q128-Flash nutzen (siehe "
              f"tools/README-img2fullscreen.md).", file=sys.stderr)


if __name__ == "__main__":
    main()
