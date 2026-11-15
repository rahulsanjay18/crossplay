# Sheet music viewer: design

**Status:** Approved for later. Not started.
**Last updated:** 2026-11-14
**Vocabulary:** [GLOSSARY.md](../../GLOSSARY.md)

## Context and scope

The Songs app shows chord Charts. This document specifies the second half: reading
sheet music, a Score, on the same device. The firmware can't open PDFs, and the
Sticky's 800 by 480 panel is smaller than a printed page.

## Goals

- Read a Score page by page in Songs, alongside Charts.
- Put Scores in Setlists next to Charts.
- Turn pages hands-free enough to keep playing.

## Non-goals

- Rendering PDF or MusicXML on the device.
- Annotation, playback, or transposing sheet music.

## Detailed design

### What a Score is on the card

A Score is a folder in `/scores`:

```text
/scores/<title>/
  page-001.png
  page-002.png
  ...
  score.txt      optional: title, composer
```

Page files sort by name. The device reads PNG and JPEG through the existing
converters to its own bitmap format.

### Making a Score from a PDF

Add `tools_local/songs/pdf_to_score.sh`, which runs on your computer or server:

```bash
pdftoppm -png -r 150 -gray input.pdf "/path/to/scores/<title>/page"
```

Then reduce each page to black and white and trim margins, so a page reads well at
1 bit per pixel. Test that dithered grays don't leave staff lines broken.

### Reading a page

- Show Scores in landscape, 800 wide by 480 high.
- Fit each page to the panel width. A portrait page is then about 2.3 screens tall.
- Step down half a screen per turn, so the line you were reading stays on screen
  as the next one appears.
- At the bottom of a page, the next step starts the next page at its top.
- Decode one page into a PSRAM buffer at a time, and decode the next page in the
  background while you read.

### Turning pages

Use the same controls as Charts: a tap anywhere or a side button moves forward;
the other side button moves back. At the end of a Score in a Setlist, move to the
next item.

### Memory

A 150 dpi letter page in 1-bit is about 1275 by 1650 pixels, about 263 KB. Keep at
most two pages decoded, about 530 KB in PSRAM. Don't keep decoded pages on the SD
card unless decoding proves too slow; if it does, cache them in
`/.crosspoint/scores/`.

## Alternatives considered

- **PDF rendering on the device.** Rejected: a PDF engine is large and slow on this
  chip, and the conversion is a solved problem off it.
- **Fit the whole page to the screen.** Rejected: notes become unreadable at 480
  pixels high.
- **Two half pages side by side.** Rejected: the eye has to jump between columns
  mid-line.

## Open questions

- Is 150 dpi enough for small notes after scaling to 800 wide? Measure with a
  dense piano score.
- Should portrait orientation be offered for pages with few systems?

## Acceptance criteria

- [ ] A 10-page Score converted by the script opens and turns through every page.
- [ ] Staff lines stay unbroken at the default contrast.
- [ ] A Setlist mixing Charts and Scores plays through in order.
- [ ] The next page appears without a visible decoding pause.
