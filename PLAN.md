# OUI Paint — plan

A raster image editor in the spirit of Paint.NET: quick to open, quick to use, nothing to learn. Built in C++ on
OUI, with JavaScript for image plugins and for hooks that run after saving. Windowed only.

License: zlib, as OUI.

## Goals

- **Instant start.** A native binary, no runtime to boot. Plugins are found at start but loaded when first used.
- **Paint.NET's feel and keyboard.** Same tools, same shortcuts, same habits.
- **Layers**, with opacity, visibility and blend modes.
- **Image operations:** flip, rotate (90° steps and free), resize, canvas size, crop to selection.
- **Selection operations too:** everything that flips, rotates or scales the image also works on the selected
  pixels alone — something Paint.NET lacks.
- **Tools:** rectangle, ellipse, lasso and magic wand selection; move selection and move selected pixels; brush,
  pencil, eraser; fill; gradient; line, curve and arrow; rectangle and ellipse shapes; text; color picker; zoom; pan.
- **Zoom and panning**, several documents open at once in tabs.
- **Copy and paste through the system clipboard**, in both directions: an image copied in a browser or a
  screenshot pastes in, and what is copied in OUI Paint pastes anywhere else.
- **Files:** PNG, JPEG, BMP, TGA, GIF to read; PNG, JPEG, BMP, TGA to write; OpenRaster (.ora) as the layered
  format; raw 16-bit grayscale heightmaps (.r16 / .raw), read and written.
- **Any pixel depth:** 8 or 16 bits per channel (and room for 32-bit float), chosen per document. Creating a new
  image asks for its depth and whether it is color or grayscale; a 16-bit grayscale image saved as PNG is a 16-bit
  grayscale PNG, ready to be used as a heightmap.
- **Indexed color:** a new image can be indexed instead, with N palettes of M colors each, as console graphics
  use them (NES, Master System, Mega Drive, SNES, Neo Geo…) without imitating any one system's rules.
- **Plugins:** effects built from C++ pieces that JavaScript puts together, with a way out to per-pixel JavaScript,
  run in parallel whenever the algorithm allows it.

## Architecture

### Pixels and tiles

A layer is a grid of tiles, 256×256 pixels each. Tiles are the unit of everything heavy:

- **Undo** stores only the tiles a step changed, never the whole layer. A brush stroke on a large image costs
  the tiles it touched.
- **Parallel work** — effects, blending, resampling — is split by tiles across cores.
- **Progressive preview:** an effect's result appears tile by tile as each is done; changing a parameter
  mid-render discards the work in flight and starts over.
- **Empty tiles** are not stored at all.

A document has one pixel format for all its layers: RGBA at 8 bits per channel, RGBA at 16 bits, grayscale at
16 bits (heightmaps), with 32-bit float RGBA designed in but not built until needed. The C++ pieces are written
once for any format (templated on the pixel type), not one copy per format. What reaches the screen is always
converted to 8 bits after the layers are blended.

### Indexed color

Indexed is one more pixel format: each pixel holds an index, not a color, so tiles, undo and parallel work are the
same as for any other format.

- **Palettes:** the document has N palettes of M colors (both chosen when creating it, and changeable later).
  Each palette can mark one index as transparent (index 0 by default).
- **Layers pick a palette slot.** A layer's pixels are indices 0..M-1 into the palette in its slot ("palette 2"),
  not into a particular palette. Switching one layer to another slot recolors that layer; reordering the palettes
  recolors every layer using the slots that moved, as reloading a console's palette memory does. Both are how
  palette swaps are tried out.
- **Editing a palette entry** recolors everything that uses it immediately.
- **Allowed colors (optional):** a set of colors the palettes may take from, such as a console's whole gamut,
  loaded from an image or a palette file. With it, the color picker offers only those colors.
- **Loading palettes and color sets:** an indexed PNG gives its palette in its exact order; a true-color image
  gives its distinct colors in reading order; JASC .pal, GIMP .gpl and Photoshop .act files are read and written.
- **Tools** paint indices, without antialiasing. Effects that mix colors (blurs, blend modes) compute in true color
  and map the result back to the layer's palette, with optional dithering. Pasting a true-color image does the same.
- **Palette files:** the whole palette setup — allowed colors, N, M, the palettes with their names — is saved to
  and loaded from a JSON file of its own, so an artist carries the same palettes from image to image and keeps
  them under version control beside the graphics. A new image can start from one.
- **Saving:** an indexed image saves either as indexed PNG or as an ordinary true-color image (RGBA, or RGB
  without transparency) in any format. Indexed PNG holds all N×M entries in their exact order — slot p, entry i at
  p×M+i — never reordered and never pruned of unused entries; reordering the palettes therefore changes the
  indices written. OpenRaster keeps the palettes and each layer's slot as its own data. Turning the result into a
  specific console's data is a job for a hook after saving, not for the editor.

### Layers and blending

Layers blend top to bottom with opacity and a blend mode (normal, multiply, screen, overlay, add, difference…).
Blending happens in the document's format; only the final result is converted for display, over a checkerboard
where it is transparent. Blended tiles are cached and redone only where something changed.

### History

Every action is one undoable step holding the tiles it replaced, plus what it did to the document's structure
(layer added, moved, resized…). A history panel lists the steps; clicking one goes back to it.

### Selection

A selection is a coverage mask (0–255 per pixel, so ellipses and feathering have soft edges), stored in tiles
like a layer. Tools and effects respect it. "Selected pixels" can be lifted into a floating piece that moves,
flips, rotates and scales freely until it is put down — this is how the selection operations work, with the same
resampling piece the image operations use.

### Drawing

An antialiased rasterizer for lines, polylines, curves, arrows, rectangles and ellipses, in C++, with stroke width,
fill and antialiasing as options. Brush strokes join consecutive pointer positions with segments, so a fast stroke
draws a line and not scattered dots.

### Clipboard

One interface with one implementation per system, as OUI's text clipboard already is. Copying offers the image as
PNG (keeping alpha). Pasting accepts PNG and the usual image types the system offers.

- **Linux first:** `wl-copy` / `wl-paste` on Wayland, `xclip` on X11, asking for `image/png`.
- **Windows later:** PNG as a registered clipboard format plus `CF_DIBV5` for older programs.

Pasting larger than the canvas asks whether to enlarge the canvas, as Paint.NET does.

### Files

Every format keeps the document's depth when the format can hold it:

| Format       | 8-bit | 16-bit | Notes                                          |
|--------------|:-----:|:------:|------------------------------------------------|
| PNG          | yes   | yes    | color or grayscale, with or without alpha      |
| OpenRaster   | yes   | yes    | its layers are PNG                             |
| TIFF         | yes   | yes    | later; common for 16-bit work                  |
| PGM / PPM    | yes   | yes    | trivial, handy for heightmaps                  |
| Raw (.r16)   | —     | yes    | grayscale only                                 |
| JPEG         | yes   | —      |                                                |
| BMP, TGA     | yes   | —      |                                                |
| GIF          | read  | —      |                                                |

Saving a 16-bit document to a format that cannot hold it says so and asks before reducing it to 8 bits; it never
does it silently. Opening a 16-bit file makes a 16-bit document.

- **Images:** stb_image reads 8 and 16-bit PNG; stb_image_write only writes 8 bits, so 16-bit PNG goes through a
  writer that supports it (lodepng or similar). PNG is written without gamma or color-profile chunks, so a
  grayscale PNG's values are the values that were painted.
- **OpenRaster (.ora):** a zip holding `stack.xml` (layer order, names, opacity, visibility, blend modes) and one
  PNG per layer. Needs zip read/write; OUI only inflates today.
- **Raw heightmaps (.r16 / .raw):** little-endian 16-bit values, no header. On open, the size is proposed from the
  file length (square when it is a square) and the user can correct it.
- **Saving** is atomic: write to a temporary file, then rename over the old one.

### JavaScript

QuickJS, as in OUI. Two uses:

1. **Image plugins.** A plugin declares its parameters (which become the effect's dialog, with live preview) and
   its body. The body normally chains C++ pieces:
   - Gaussian blur (separable) and box blur
   - sampling along a path: motion, radial and zoom blur
   - convolution with a small matrix: edge detect, sharpen, emboss
   - fractal noise: clouds
   - blend two images with a blend mode
   - lookup tables per channel: curves, levels, hue/saturation, invert, posterize
   - resample: resize and rotate
   - masks, so an effect stays inside the selection

   When no piece fits, a plugin can process pixels in JavaScript, row by row (a call per pixel would cost more
   than the work). A plugin that declares that it only reads the source and writes the destination — never reads
   what it is writing — runs in parallel: one JavaScript runtime per core, each on its own tiles, with a margin of
   neighboring pixels so blurs and convolutions can be written in JavaScript too. Plugins see pixel values as
   numbers from 0 to 1 whatever the document's format.

2. **Hooks after saving.** A script that runs when a file is saved, to convert, copy or notify.

### Keyboard

Paint.NET's shortcuts, transcribed from its documentation rather than from memory: tools by letter (pressing the
letter again cycles the tools that share it), and the menus' Ctrl combinations for files, editing, selection, image
and layers. They are OUI commands, so they also show in the command palette.

## What OUI needs

OUI Paint brings in OUI as a git submodule; what follows is done in OUI itself, as general features.

- **Pointer in pixels.** Mouse events carry cell positions today, and only a horizontal offset inside the cell.
  The window backend knows the exact pixel position; events should carry it too, relative to the widget.
- **Pointer shapes.** The window can only switch to the column-resize pointer today. Needed: crosshair, hand,
  move, and custom cursors per tool.
- **Image widget with zoom.** Drawing a picture today copies it one-to-one. Needed: zoom (sharp pixels when
  enlarged, filtered when reduced), panning, and a checkerboard under transparency.
- **New widgets:** slider; numeric field with arrows and dragging; color picker (hue/saturation square, alpha,
  hex) with primary/secondary colors and a palette; tool bar with icons and a tool options bar; layer list with
  thumbnails, visibility and opacity; history list.
- **SVG icons** through nanosvg, so icons stay sharp at any interface scale.
- **Clipboard with images**, beside the text it carries today.
- **Zip writing** (and deflate), for OpenRaster.

## Icons

Godot's editor icons (MIT), copied into the repository with their license notice. They already cover bucket,
eraser, color picker, gradient, line, rectangle, mirror X/Y, rotate left/right, zoom, move, pan, select, save
and load. Missing ones are drawn in the same style.

## Steps

0. **OUI groundwork:** pointer in pixels, pointer shapes, image widget with zoom, SVG icons, slider and numeric
   field. OUI Paint as a submodule-based project that opens a window.
1. **Documents:** new image dialog with depth (8/16 bits) and color/grayscale; open and save PNG at both depths,
   JPEG/BMP/TGA, raw heightmaps; zoom and pan, tabs, tiles and pixel formats.
2. **Layers and history:** layer panel, blend modes, undo/redo with tiles, OpenRaster. With a brush, this is
   already enough to paint heightmaps.
3. **Selection and clipboard:** rectangle and ellipse selection, move selection and pixels, copy/cut/paste
   through the system clipboard. From here it is useful for everyday work on screenshots.
4. **Drawing:** brush, pencil, eraser, fill, gradient, lines, arrows, shapes, text, color picker; color widgets.
5. **Transforms:** image and selection flip/rotate/resize, canvas size, crop; lasso and magic wand.
6. **Effects and plugins:** the C++ pieces, the JavaScript plugin API, parallel per-pixel plugins, hooks after
   saving; the blurs, edge detect and clouds as the first plugins.
7. **Indexed color:** palettes, layers picking a slot, reordering, allowed colors, JSON palette setups, palette
   files, indexed and true-color saving.
8. **More formats:** TIFF, PGM/PPM, 32-bit float if needed.
9. **Windows:** clipboard, pointers and anything else system-specific.
