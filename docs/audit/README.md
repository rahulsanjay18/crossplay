# Firmware memory audit, 2026-11-08

Build: `gh_release_sticky` with Shogi and Xiangqi applied. ESP32-S3, 16 MB flash,
8 MB PSRAM. Open items are in [checklist-2026-11-08.md](checklist-2026-11-08.md).

| Measure | Value |
| --- | --- |
| `firmware.bin` | 7,730,528 of 8,323,072 bytes (92.9%), about 592 KB free |
| Static RAM (`.data` + `.bss`) | 159,640 of 327,680 bytes (48.7%) |
| Worst-case stack, render task | 11,104 of 16,384 bytes, 5,280 headroom |
| Stack budget | all 4 tasks fit; recursion and indirect calls are not fully visible to it |
| Every app removed but Minesweeper and Notes | saves 1,431,968 flash, 83,432 static RAM |

## Files

- `app-memory-2026-11-08.csv`: one row per shelf app, then shared app libraries.
- `firmware-groups-2026-11-08.csv`: the whole image by app, library and SDK.

## Columns in the app CSV

| Column | Source | Meaning |
| --- | --- | --- |
| `flash_saved_if_disabled_bytes` | relink with the shelf row removed | True saving. Includes template code and strings only that app reached. |
| `ram_static_data_bytes`, `ram_static_bss_bytes`, `iram_bytes` | same relinks | Internal RAM. |
| `ram_permanent_total_bytes` | data + bss | Held from boot, **even if the app is never opened**. |
| `activity_sizeof_bytes` | DWARF in the target objects | Real 32-bit size of the Activity object. |
| `activity_heap_region` | `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096` | Over 4,096 bytes `malloc` uses PSRAM. |
| `extra_open_heap_bytes`, `_note` | allocation sites in the source | Known fixed allocations beyond the Activity. 0 with a note means content dependent. |
| `sim_open_live_heap_delta_bytes` | simulator, malloc tracker | Live heap 6 s after opening, minus Home. |
| `map_code_bytes`, `map_rodata_bytes` | link map by object | What the app's own objects contain. |
| `sd_movable`, `sd_note` | judgement, from the largest objects | Whether this flash could live on the SD card. |

## Reading it

- Permanent RAM is the scarce number. Go (36 KB: the MCTS tables in `michi/`) and
  Toy Battle (24 KB: `ToyBattleBrain.cpp`) cost more than every other app combined,
  and could be allocated on open.
- Biggest while open: Hex 2.1 MB (PSRAM pool), Underhand 43 KB, Go 38 KB,
  Shogi 31 KB, Chess 15 KB.
- Biggest flash: Underhand 331 KB, about 4 times anything else among the games.

## SD card

SD can take flash pressure off, not RAM pressure. Outside the apps, the largest
candidates by far are in `firmware-groups`:

| Data | Bytes | Note |
| --- | --- | --- |
| Reader fonts in `src/main.cpp` | about 2.2 MB | SD font loading (`SdCardFontSystem`) already exists |
| UI strings, all languages | 404 KB | or shrink with `custom_i18n_builtin_langs` |
| Hyphenation tries | 352 KB | German alone is 206 KB |
| Toybox fonts | 219 KB | used on every game screen, poor candidate |
| Web UI assets | 117 KB | served rarely |

Moved data must be read into RAM or streamed, so each move trades flash for
latency and possibly heap. The SD layer is built with exFAT support and 64-bit
capacity. Not tested on the 512 GB card from here.

## Per-app build flags

Removing a shelf row is enough for the linker to drop the app, with no source
deleted, which the sweep demonstrates 34 times. Two limits: `CrossPointWebServer`
and `SleepActivity` still use `NotesCore`, `NotesSleep` and `WallpapersCore`, so
those survive disabling Notes or Wallpapers; and doc-claims tests compare the
README and site with the shelf rows. Not implemented yet, see the checklist.

## Limits

- Sweep deltas are single removals and do not add up exactly: shared code stays
  until the last user goes (sum of rows 1,421,552, all-off 1,431,968 with two apps kept).
- `map_rodata_bytes` understates apps. The linker merges strings and credits the
  whole block to one object, which is why `Wire` shows 304 KB in the groups file.
- The simulator is 64-bit, has an empty SD card, and measures the opening menu
  only: no game in progress, and no search running. Use it to compare apps, not
  as device numbers. Peak heap was hidden by a 10 MB boot transient, so only live
  heap is reported. No board was attached during this audit.

## Reproducing

Build `gh_release_sticky`, then for each `shelf::Item` delete its row and include
from a scratch copy of `Shelf.cpp`, relink, and diff `firmware.bin`,
`.dram0.data` and `.dram0.bss`. Stack: `PLATFORMIO_BUILD_FLAGS="-fstack-usage
-fcallgraph-info=su"` with an empty build cache, then `scripts_local/stack_budget.py`.
Simulator: `CROSSPLAY_AUTOSTART=<title>` under an `LD_PRELOAD` malloc counter.
