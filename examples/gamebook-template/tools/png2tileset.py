#!/usr/bin/env python3
"""
png2tileset.py
Convert an image into a C sprite16_t tileset + an index grid, e.g.:

const sprite16_t forest_bg_tiles[7] = {
    /* tile 0 */
    { .px = { {6,6,6,...}, ... } },
    ...
};

const uint8_t forest_bg_grid[5][20] = {
    {0,0,1,2,0,...},
    ...
};

This is the engine's only "large picture" primitive - draw_sprite16()
only knows how to blit a single 16x16, 1-byte-per-pixel, paletted tile
(see include/sprite16.h). There is no image decoder, filesystem, or SD
card support on this freestanding (-nostdlib) target, so a picture has
to become compiled-in C data - this script does that conversion offline,
the same way tools/midi2console.py turns a MIDI file into a MusicNote[]
for music.h. The output is NOT wired into art.c/tileset16.c automatically
(nothing here knows about nebelkrone's Scene/bg_id_t plumbing) - see
README-png2tileset.md for how to actually draw the result.

Requires:
    python3 -m pip install pillow

Usage:
    ./png2tileset.py scene.png -o scene.c
    ./png2tileset.py scene.png -o scene.c --palette-from ../../../src/framebuffer8.c
    ./png2tileset.py sprite.png -o sprite.c --chroma-key FF00FF
    ./png2tileset.py photo.png -o photo.c --dither --pad
    ./png2tileset.py --list-palette
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

# The engine's default 16-color palette (src/framebuffer8.c, base_palette[]),
# as RGB565. Kept in sync by hand; pass --palette-from to read it straight
# out of a framebuffer8.c instead of trusting this copy.
DEFAULT_PALETTE_RGB565 = [
    0x0000, 0xFFFF, 0x8800, 0x0639, 0xC897, 0x04A8, 0x0015, 0xEEE7,
    0xDD86, 0x6222, 0xFB2C, 0x4228, 0x8C51, 0x8FF1, 0x777F, 0xBDD7,
]

PALETTE_NAMES = [
    "Black", "White", "Red", "Cyan", "Purple", "Green", "Blue", "Yellow",
    "Orange", "Brown", "Light Red", "Dark Grey", "Grey", "Light Green",
    "Light Blue", "Light Grey",
]


def rgb565_to_rgb888(v: int) -> tuple[int, int, int]:
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def load_palette_from_source(path: Path) -> list[int]:
    """Pulls the 16 base_palette[] hex values straight out of a
    framebuffer8.c, in array order, so this tool can't drift out of sync
    with the engine's actual palette."""
    text = path.read_text(encoding="utf-8")
    m = re.search(r"base_palette\[16\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        print(f"Fehler: Kein 'base_palette[16] = {{...}}' in {path} gefunden.",
              file=sys.stderr)
        sys.exit(1)
    values = re.findall(r"0x[0-9A-Fa-f]{1,4}", m.group(1))
    if len(values) != 16:
        print(f"Fehler: {len(values)} Werte statt 16 in base_palette[] "
              f"gefunden ({path}).", file=sys.stderr)
        sys.exit(1)
    return [int(v, 16) for v in values]


def nearest_index(rgb: tuple[int, int, int], palette_rgb: list[tuple[int, int, int]]) -> int:
    r, g, b = rgb
    best_i, best_d = 0, None
    for i, (pr, pg, pb) in enumerate(palette_rgb):
        d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if best_d is None or d < best_d:
            best_i, best_d = i, d
    return best_i


def quantize(img: Image.Image, palette_rgb: list[tuple[int, int, int]],
             alpha_threshold: int, chroma_key: tuple[int, int, int] | None,
             dither: bool) -> list[list[int]]:
    """Returns a [h][w] grid of palette indices (0 = transparent/black)."""
    w, h = img.size
    px = img.load()

    # Working float RGB buffer for optional Floyd-Steinberg error diffusion,
    # and a parallel transparency mask so transparent pixels neither receive
    # nor propagate quantization error.
    work = [[list(px[x, y][:3]) for x in range(w)] for y in range(h)]
    transparent = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < alpha_threshold:
                transparent[y][x] = True
            elif chroma_key is not None and (r, g, b) == chroma_key:
                transparent[y][x] = True

    out = [[0] * w for _ in range(h)]

    for y in range(h):
        for x in range(w):
            if transparent[y][x]:
                out[y][x] = 0
                continue

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
                if 0 <= nx < w and 0 <= ny < h and not transparent[ny][nx]:
                    work[ny][nx][0] += er * frac
                    work[ny][nx][1] += eg * frac
                    work[ny][nx][2] += eb * frac

            spread(x + 1, y,     7 / 16)
            spread(x - 1, y + 1, 3 / 16)
            spread(x,     y + 1, 5 / 16)
            spread(x + 1, y + 1, 1 / 16)

    return out


def slice_tiles(index_grid: list[list[int]]) -> tuple[list[tuple[int, ...]], list[list[int]]]:
    """Cuts a [h][w] index grid into 16x16 tiles, deduplicating identical
    ones. Returns (unique_tiles, [rows][cols] grid of indices into it)."""
    h = len(index_grid)
    w = len(index_grid[0])
    rows, cols = h // TILE, w // TILE

    seen: dict[tuple[int, ...], int] = {}
    tiles: list[tuple[int, ...]] = []
    grid = [[0] * cols for _ in range(rows)]

    for ty in range(rows):
        for tx in range(cols):
            flat = []
            for sy in range(TILE):
                row = index_grid[ty * TILE + sy]
                flat.extend(row[tx * TILE:tx * TILE + TILE])
            key = tuple(flat)
            if key not in seen:
                seen[key] = len(tiles)
                tiles.append(key)
            grid[ty][tx] = seen[key]

    return tiles, grid


def pad_to_multiple(img: Image.Image) -> Image.Image:
    w, h = img.size
    nw = (w + TILE - 1) // TILE * TILE
    nh = (h + TILE - 1) // TILE * TILE
    if (nw, nh) == (w, h):
        return img
    padded = Image.new("RGBA", (nw, nh), (0, 0, 0, 0))
    padded.paste(img, (0, 0))
    return padded


def c_identifier(name: str) -> str:
    ident = re.sub(r"\W", "_", name)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    return ident or "tileset"


def emit_c(tiles: list[tuple[int, ...]], grid: list[list[int]],
           tileset_name: str, grid_name: str, source_name: str) -> str:
    lines = []
    lines.append("/**")
    lines.append(f" * @file (generated)")
    lines.append(f" * @brief Auto-generated by tools/png2tileset.py from")
    lines.append(f" *        '{source_name}' - do not hand-edit, re-run the script instead.")
    lines.append(" */")
    lines.append('#include "sprite16.h"')
    lines.append("")
    lines.append(f"const sprite16_t {tileset_name}[{len(tiles)}] = {{")
    for i, tile in enumerate(tiles):
        lines.append(f"    /* tile {i} */")
        lines.append("    { .px = {")
        for r in range(TILE):
            row = tile[r * TILE:(r + 1) * TILE]
            lines.append("        {" + ",".join(str(v) for v in row) + "},")
        lines.append("    } },")
    lines.append("};")
    lines.append("")

    rows, cols = len(grid), len(grid[0])
    lines.append(f"/** @brief {rows}x{cols} grid of indices into {tileset_name}[], row-major. */")
    lines.append(f"const uint8_t {grid_name}[{rows}][{cols}] = {{")
    for row in grid:
        lines.append("    {" + ",".join(str(v) for v in row) + "},")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def emit_h(tileset_name: str, grid_name: str, rows: int, cols: int, guard: str) -> str:
    lines = []
    lines.append("/**")
    lines.append(f" * @file (generated)")
    lines.append(" * @brief Auto-generated by tools/png2tileset.py - do not hand-edit.")
    lines.append(" */")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append('#include "sprite16.h"')
    lines.append("")
    lines.append(f"#define {grid_name.upper()}_ROWS {rows}")
    lines.append(f"#define {grid_name.upper()}_COLS {cols}")
    lines.append("")
    lines.append(f"extern const sprite16_t {tileset_name}[];")
    lines.append(f"extern const uint8_t {grid_name}[{rows}][{cols}];")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def parse_hex_color(s: str) -> tuple[int, int, int]:
    s = s.strip().lstrip("#")
    if len(s) != 6:
        raise argparse.ArgumentTypeError("Farbe muss RRGGBB sein, z.B. FF00FF")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", nargs="?", help="Source image (PNG recommended)")
    ap.add_argument("-o", "--output", help="Output .c file path")
    ap.add_argument("--header", help="Also write a matching .h (default: same "
                     "path as -o with .h instead of .c)")
    ap.add_argument("--no-header", action="store_true", help="Skip writing a header")
    ap.add_argument("--tileset-name", help="C identifier for the tile array "
                     "(default: derived from the output filename)")
    ap.add_argument("--grid-name", help="C identifier for the index grid "
                     "(default: <tileset-name>_grid)")
    ap.add_argument("--palette-from", type=Path, help="Read the 16-color "
                     "palette straight out of this framebuffer8.c instead "
                     "of the copy built into this script")
    ap.add_argument("--chroma-key", type=parse_hex_color, help="Treat this "
                     "RRGGBB color as transparent (index 0), for source "
                     "images with no alpha channel")
    ap.add_argument("--alpha-threshold", type=int, default=128, help="Alpha "
                     "values below this (0..255) count as transparent "
                     "(default: 128)")
    ap.add_argument("--pad", action="store_true", help="Pad the image with "
                     "transparent pixels up to the next 16px multiple "
                     "instead of erroring on a non-multiple-of-16 size")
    ap.add_argument("--dither", action="store_true", help="Floyd-Steinberg "
                     "dither before quantizing - helps photos/gradients, "
                     "usually looks worse than plain quantizing on flat "
                     "pixel art")
    ap.add_argument("--list-palette", action="store_true", help="Print the "
                     "16 palette colors and exit")
    args = ap.parse_args()

    palette565 = (load_palette_from_source(args.palette_from)
                  if args.palette_from else DEFAULT_PALETTE_RGB565)
    palette_rgb = [rgb565_to_rgb888(v) for v in palette565]

    if args.list_palette:
        for i, (v, rgb) in enumerate(zip(palette565, palette_rgb)):
            name = PALETTE_NAMES[i] if i < len(PALETTE_NAMES) else "?"
            print(f"{i:2d}  0x{v:04X}  RGB{rgb}  {name}")
        return

    if not args.input or not args.output:
        ap.error("input and -o/--output are required (unless --list-palette)")

    in_path = Path(args.input)
    out_path = Path(args.output)

    img = Image.open(in_path).convert("RGBA")

    w, h = img.size
    if w % TILE or h % TILE:
        if args.pad:
            img = pad_to_multiple(img)
            print(f"Hinweis: {w}x{h} auf {img.size[0]}x{img.size[1]} "
                  f"aufgefuellt (transparent).", file=sys.stderr)
        else:
            print(f"Fehler: Bildgroesse {w}x{h} ist kein Vielfaches von "
                  f"{TILE}x{TILE}. Mit --pad automatisch auffuellen, oder "
                  f"das Bild vorher zuschneiden/skalieren.", file=sys.stderr)
            sys.exit(1)

    index_grid = quantize(img, palette_rgb, args.alpha_threshold,
                           args.chroma_key, args.dither)
    tiles, grid = slice_tiles(index_grid)

    tileset_name = c_identifier(args.tileset_name or out_path.stem + "_tiles")
    grid_name = c_identifier(args.grid_name or tileset_name.replace("_tiles", "") + "_grid")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(emit_c(tiles, grid, tileset_name, grid_name, in_path.name),
                         encoding="utf-8")

    if not args.no_header:
        h_path = Path(args.header) if args.header else out_path.with_suffix(".h")
        guard = c_identifier(h_path.stem).upper() + "_H"
        h_path.write_text(emit_h(tileset_name, grid_name, len(grid), len(grid[0]), guard),
                           encoding="utf-8")
    else:
        h_path = None

    rows, cols = len(grid), len(grid[0])
    print(f"{in_path.name}: {img.size[0]}x{img.size[1]} px -> "
          f"{rows}x{cols} tiles, {len(tiles)} unique of {rows*cols} total")
    print(f"  wrote {out_path}"
          + (f" and {h_path}" if h_path else ""))
    print(f"  tileset_name = {tileset_name}, grid_name = {grid_name}")


if __name__ == "__main__":
    main()
