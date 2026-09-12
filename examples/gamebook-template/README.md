# gamebook-template

A story-neutral starting point for a new illustrated interactive-fiction
gamebook on this engine - the same mechanics `examples/nebelkrone/`
("Die Nebelkrone") is built on, minus its actual story. Run it as-is and
you get a short, obviously-placeholder adventure exercising every
mechanic once; replace the content and you have your own.

## Starting a new adventure

1. Copy this whole directory to `examples/your-game-name/`.
2. Add `examples/your-game-name` to `EXAMPLES` in the repo root `Makefile`.
3. Write your own story:
   - **`story.h`**: `GAME_TITLE`/`GAME_TAGLINE`/`GAME_PREMISE_1..3` (the
     title screen text), hero starting stats, your own `StoryFlag`
     names (keep `FLAG_HEAL_ITEM` if you want the combat heal-item
     option, drop it if not), your `Enemy` table, and your own
     `SCENE_*` names (keep `SCENE_INTRO` and `SCENE_END_LOSE`, both
     required - see the header comment for why).
   - **`story.c`**: the actual scene graph - replace the eight
     placeholder scenes with as many of your own as the story needs.
     See `examples/nebelkrone/story.c` for what a complete ~15-scene
     adventure looks like built the same way.
4. Optionally replace the art and music:
   - **`art.c`/`art.h`/`tileset16.c`**: the included tiles are generic
     fantasy-adventure locations (village, swamp, ruin, combat/victory
     chambers, death) - usable as-is, or extend `bg_id_t`/`tileset16[]`
     with your own, or convert your own artwork with
     `tools/png2tileset.py` (see `tools/README-png2tileset.md`).
   - **`track.c`/`track.h`**: swap in your own tune (melody + optional
     bass, see `music_init()`/`music_init_bass()` in `main.c`), or keep
     the default.
5. `main.c` normally doesn't need to change at all - it only reads data
   and constants out of `story.h`/`story.c`.

## What's fixed vs. what's yours

| Fixed (the engine) | Yours (per story) |
|---|---|
| Three attributes: MUT, KLUGHEIT, GEWANDTHEIT | Starting values for each |
| d20-under-attribute checks | Which checks appear, and their consequences |
| Combat resolved against GEWANDTHEIT | The enemy table, and whether combat appears at all |
| The five `NodeKind`s (TEXT/CHECK/COMBAT/END_WIN/END_LOSE) | The scene graph built from them |
| UP/DOWN+BTN menu navigation | The choice text itself |

## Building

```bash
make            # firmware.elf / firmware.bin
make flash      # flash to the console (needs openocd + cmsis-dap)
```

Same standalone Makefile pattern as every other example - see the repo's
`dokumentation.org` for the full engine writeup.
