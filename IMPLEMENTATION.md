# OUI Paint — implementation plan

How [PLAN.md](PLAN.md) gets built: repository layout, the core types, what goes into OUI and what stays here, and
each milestone with its tasks and what "done" means. The early milestones are detailed; the later ones get detailed
when they are reached.

## Repository layout

```
OUI-Paint/
  CMakeLists.txt
  third_party/
    olagarro/          git submodule (OUI and friends)
    stb/               stb_image.h, stb_image_write.h
    lodepng/           16-bit PNG writing
    miniz/             zip for OpenRaster
  src/
    main.cpp
    core/              pixel formats, tiles, layers, documents, history, selection
    codecs/            PNG, JPEG, BMP, TGA, raw heightmaps, OpenRaster
    render/            layer blending, the view (zoom, pan, checkerboard)
    tools/             one file per tool
    ui/                main window, dialogs, panels
    plugins/           JavaScript plugin host (milestone 6)
  icons/               Godot editor SVG icons + their LICENSE
  tests/               GoogleTest, one file per unit
```

## Build

- CMake + Ninja, C++17, warnings as errors, the same flags as Olagarro.
- `add_subdirectory(third_party/olagarro)` with `OLAGARRO_BUILD_TESTS=OFF`, `OLAGARRO_BUILD_EXAMPLES=OFF`,
  `OLAGARRO_OUI_WINDOW=ON`. If Olagarro does not work as a subdirectory, that is fixed in Olagarro.
- The submodule points at the Gitea mirror of Olagarro.
- Release builds link SDL3 statically, as OUI Code does: one binary, nothing to install.

## Conventions

Olagarro's [CONTRIBUTING.md](third_party/olagarro/CONTRIBUTING.md) applies unchanged: `PascalCase` types,
`snake_case` everything else, `m_` members, braces on their own line, `++ i`, `const` locals, English everywhere,
formatted with `tools/format_cpp.js`.

## Core types

### Pixel formats

```cpp
enum class PixelFormat { gray8, rgba8, gray16, rgba16 };   // later: indexed8, rgba32f
```

Pixels are stored straight (not premultiplied), channels in R, G, B, A order. Algorithms are templates over a pixel
type (`Gray8`, `Rgba8`, `Gray16`, `Rgba16`) and are reached through one dispatch function that turns a
`PixelFormat` into the matching type, so each piece is written once.

Conversions between formats are explicit functions, used when opening, saving and pasting. Nothing converts
silently.

### Tiles

- 256×256 pixels, one format, held by `std::shared_ptr<const Tile>`.
- **Copy on write:** a tool that writes to a tile shared with history clones it first. A layer's state at any
  moment is its map of tile pointers, so a history step costs the pointers of the tiles it replaced.
- A layer keeps tiles in a map keyed by tile coordinates. A missing tile is fully transparent (or the background
  value, for grayscale without alpha).

### Layers and documents

- `Layer`: name, visible, opacity, blend mode, tiles.
- `Document`: size, format, layers, active layer, selection, file path and format, history, modified flag.
- Changing the canvas size never touches tiles that stay inside; resizing the image resamples every layer.

### History

A step is `{ name, per-layer tiles replaced, structural change }`. Undo swaps the stored tiles back in; redo swaps
them again. Structural changes (add, delete, move, merge layers; canvas size) store what they need to undo
themselves. Memory used by history is shown in the history panel and capped by a setting, dropping the oldest
steps.

### Selection

A coverage mask (8-bit, tiles like a layer) plus its outline for drawing the marching ants. A floating selection is
a temporary layer with a transform (translation, rotation, scale, flip) applied when it is put down.

### Rendering

- **Blend:** per tile, visible layers top to bottom into an RGBA 8-bit result, cached per tile and redone only
  for dirty tiles. 16-bit documents blend in 16 bits and convert at the end.
- **View:** takes the blended tiles and produces exactly the pixels of the visible viewport: nearest neighbour at
  100% and above, box filtering below, checkerboard under transparency, selection outline and tool overlays on top.
  The result is a buffer the size of the widget, handed to OUI's `draw_image`, which copies it 1:1.
- Zoom steps as Paint.NET: 1% … 3200%; Ctrl+wheel zooms around the pointer, wheel scrolls, middle button or Space
  drag pans.

## What goes into OUI

Each item is done in Olagarro, with tests in its own suite, as a general feature, before OUI Paint uses it.

| Item | What |
|---|---|
| Pointer in pixels | `MouseEvent` carries the pointer's position in pixels, relative to the widget, besides the cell. Terminal backends leave it as the cell's corner. |
| Widget pixel geometry | A widget can ask its area in pixels and the cell size, to draw pixel content that fits exactly. |
| Drag capture | A drag that starts in a widget keeps reporting to it outside its area and outside the window. |
| Pointer shapes | More system pointers (crosshair, hand, closed hand, move, text, none) and custom pointers from an image with a hot spot. |
| SVG icons | nanosvg rasterizes SVG to pixels at any size, cached per size; buttons and menus can show them. |
| Slider | Horizontal slider with value, range, step, keyboard and wheel. |
| Numeric field | Number entry with arrows, wheel, and dragging on its label. |
| Clipboard with data | `set_data(type, bytes)`, `types()`, `data(type)` beside the text calls. Linux: `wl-copy --type` / `wl-paste --type`, `xclip -t`. Windows later. |

Codecs, zip, colors widgets and everything specific to painting stay in OUI Paint.

## Testing

- **Unit tests** (GoogleTest) for everything in `core/`, `codecs/` and `render/`: round trips per format and
  depth, copy on write, undo/redo, blending against hand-computed values, view sampling at several zooms.
- **Interface tests** through OUI's in-memory pixel backend: drive the application with events, read the pixels
  it painted. No window is ever opened on the user's desktop; visual checks run under `xvfb-run` and are saved
  as PNG.
- Every bug fix comes with a test seen failing first.
- The whole suite passes before a milestone is called done.
- Builds run with `nice -n 19` and `-j8`.

## Milestones

### 0 — OUI groundwork and an empty window

1. Olagarro as a submodule; CMake skeleton; `oui_paint` opens a window with a menu bar, an empty tab area and a
   status bar; an empty test target runs.
2. OUI: pointer in pixels, widget pixel geometry, drag capture.
3. OUI: pointer shapes.
4. OUI: SVG icons; Godot icons copied into `icons/` with their license; a tool bar showing a few.
5. OUI: slider and numeric field.

Done: the window opens under xvfb with tool bar icons; a test widget receives pixel positions and changes the
pointer; Olagarro's and OUI Paint's suites pass.

### 1 — Documents

1. Pixel formats, tiles with copy on write, layers (one layer for now), documents.
2. Codecs: read PNG (8 and 16 bits, gray and color), JPEG, BMP, TGA, GIF (first frame); write PNG (8 and 16 bits,
   gray and color, no gamma chunks), JPEG (quality), BMP, TGA; raw 16-bit heightmaps with a size dialog on open.
   Saving to a format that cannot hold the document's depth asks before reducing. Atomic save.
3. New image dialog: width, height, depth (8/16), color or grayscale, background (white, black, transparent).
4. View: zoom, pan, scroll bars, checkerboard, zoom to fit, actual size.
5. Tabs for several documents; closing a modified one asks; status bar with pointer position, image size, zoom.
6. Menus and shortcuts for all of the above (File, View).

Done: open/save round trips are lossless at every depth in tests, a 16-bit grayscale PNG saved and reopened keeps
every value; zooming and panning a large image stay smooth.

### 2 — Layers, history, a brush

1. Several layers; layers panel with thumbnails, visibility, opacity, blend mode, add/delete/duplicate/move/merge.
2. Blending with blend modes, per-tile cache.
3. History with copy-on-write tiles; history panel; Ctrl+Z / Ctrl+Y.
4. OpenRaster read and write with miniz.
5. Round brush with size, hardness and antialiasing, joining pointer positions; eraser; color picker; primary and
   secondary colors (simple picker for now).

Done: painting a 16-bit grayscale heightmap, undoing, saving as 16-bit PNG and as OpenRaster, and reopening both,
gives back exactly what was painted.

### 3 — Selection and clipboard

1. Rectangle and ellipse selection, with Shift/Ctrl/Alt modes (add, subtract, intersect), select all, deselect,
   invert.
2. Marching ants.
3. Move selection outline; move selected pixels (floating selection), put down on Enter or on another tool.
4. OUI clipboard with data; copy, cut, paste, paste into new layer, paste into new image, copy merged. Pasting
   bigger than the canvas asks to enlarge it.
5. Crop to selection, erase selection, fill selection.

Done: an image copied in a browser pastes in; a selection copied here pastes into another program with its
transparency.

### 4 — Drawing tools

Pencil, fill (tolerance, contiguous or global), gradient (linear, radial, with transparency mode), line/curve with
arrows, rectangle and ellipse (outline, fill, both), text, full color picker with palette; tool options bar.

### 5 — Transforms

Flip, rotate 90°/180° and free rotation, resize (nearest, bilinear, bicubic), canvas size with anchor — for the
image, the layer and the floating selection alike. Lasso and magic wand.

### 6 — Effects and plugins

C++ pieces (see PLAN.md), plugin API in JavaScript, effect dialogs generated from declared parameters with live
progressive preview, parallel per-row JavaScript with one runtime per core, hooks after saving. First plugins:
gaussian, motion, radial and zoom blur, edge detect, sharpen, emboss, clouds, curves, levels, hue/saturation,
invert, posterize.

### 7 — Indexed color

As PLAN.md describes: palettes and slots, reordering, allowed colors, JSON palette setups, palette files, indexed
and true-color saving.

### 8 — More formats

TIFF, PGM/PPM; 32-bit float with EXR, HDR, PFM and view exposure, if needed.

### 9 — Windows

Clipboard, pointers, file associations, anything else system-specific.

## Shortcuts

Paint.NET's keyboard and mouse reference is transcribed into `src/ui/shortcuts.cpp` from its documentation
before milestone 3, and every command added afterwards takes its Paint.NET key.
