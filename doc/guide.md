# OUI Paint

A raster image editor in the spirit of Paint.NET: layers, selections that can be moved, turned and scaled, Paint.NET's
tools and keys, 8 and 16 bits per channel, indexed images with palettes, and effects written as plugins.

## The window

- **Tools** run down the left, with the primary and secondary colors and a palette of colors under them. A tool's
  letter picks it; pressing it again goes to the next tool sharing it, as S goes through the selection tools.
- **The tool's options**, such as the brush width, sit in the bar under the menus.
- **Each open image** has a tab over the canvas. Ctrl+Tab goes to the next one.
- **The history** is at the top right: the image as it was first, then each step. A click on one takes the image
  back or forward to it, and the arrows under them undo and redo. F6, or View > History Panel, hides and shows it.
- **Layers** are under it, and, in an indexed image, its palettes under them. F7, or View > Layers Panel, hides and
  shows the whole right side. Both stay as they were left the next time.
- **The status bar** says where the pointer is in the image, its size and kind, and the zoom.

The left button paints with the primary color and the right button with the secondary one. View > Bigger Interface
and Smaller Interface change the size of everything but the image; the size is kept for the next time.

## Moving around

| Keys or mouse | What it does |
|---|---|
| Ctrl+wheel | Zoom around the pointer |
| Ctrl++, Ctrl+- | Zoom in, zoom out |
| Ctrl+B | Zoom to fit the window |
| Ctrl+Shift+B | Zoom to what is selected |
| Ctrl+0 | Actual size |
| Wheel, Shift+wheel | Scroll up and down, sideways |
| Space+drag, middle button drag, or the Pan tool (H) | Pan |

## Selections

Rectangle Select, Lasso Select, Ellipse Select and the Magic Wand choose what the other tools and the effects work
on. While dragging:

| Mouse | What it does |
|---|---|
| Drag | A new selection |
| Ctrl+drag | Add to the selection |
| Alt+drag | Take away from it |
| Alt+right drag | Keep only what both cover |
| Ctrl+right drag | Invert what it covers |
| Shift | Keep a rectangle or an ellipse square or round |

Ctrl+A selects everything, Ctrl+D deselects, Ctrl+I inverts. Delete erases what is selected and Backspace
fills it with the primary color.

**Move Selected Pixels** (M) lifts what is selected so it can be dragged, scaled by its handles and turned by
dragging with the right button; Shift keeps its proportions while scaling and turns it in steps of 15 degrees. **Move Selection** (M again) moves only the outline.
Edit > Flip Selection and Rotate Selection flip and turn what is selected in place.

## Painting

| Tool | Key | What it does |
|---|---|---|
| Paintbrush | B | Soft or hard by its hardness, smooth edges with antialiasing |
| Pencil | P | One pixel wide, never smoothed |
| Eraser | E | Paints transparency |
| Paint Bucket | F | Fills the area of similar color |
| Gradient | G | From where the drag starts to where it ends |
| Clone Stamp | L | Ctrl+click picks where it copies from, then painting copies from there |
| Recolor | R | Changes the secondary color into the primary one where it paints; the right button, the other way round |
| Color Picker | K | Takes the color under the pointer: left for the primary, right for the secondary |
| Text | T | Types text where clicked |
| Line / Curve | O | Drag from one end to the other, Shift keeping to steps of 15 degrees; then drag its four handles to bend it, and Enter, or a click away from them, leaves it as it is |
| Shapes | O again | Drag from one corner to the other; Shift keeps them square or round |

[ and ] make the brush narrower and wider; with Ctrl, by 5. X swaps the primary and secondary colors. C chooses which
of them a click on a palette sets, the one framed in front; a right click sets the other.

## Layers

Each layer has a name, can be shown or hidden, and has an opacity and a blend mode, as Paint.NET's (F4 opens its
properties). The buttons under the list add, delete, duplicate, merge down and move the layer up and down. Saving
to a format of one layer, such as PNG, saves the image flattened; the layers stay in the open image, and OpenRaster
(.ora) keeps them.

## Depth

A new image is 8 or 16 bits per channel, in color or in grayscale. 16 bits keeps fine steps that 8 bits loses, as in
a heightmap or a smooth gradient; PNG saves 16 bits, JPEG and BMP only 8, and saving asks before losing the
difference.

## Indexed images and palettes

In an indexed image each pixel does not hold a color: it holds a number, the place of its color in a palette.

- **An image can have several palettes of the same size.** Each layer uses one of them. A new indexed image asks how
  many palettes and how many colors in each.
- **The palettes panel**, under the layers, shows each palette as a row of colors. The palette of the layer being
  worked on is marked with ">"; clicking another palette's number gives it to the layer.
- **Clicking a color** picks it to paint with, the left button for the primary and the right one for the secondary.
  Double-clicking it changes the color: every pixel using it changes with it.
- **The see-through color** is shown with "/". Painting with it erases.
- **Painting with any other color** uses the palette's nearest color: an indexed image never gets colors outside its
  palettes, and is never smoothed.
- **Reordering palettes** (Image > Palettes > Move the Layer's Palette Up or Down) moves them with what uses them,
  which changes how the image looks when it is saved as an indexed PNG of every palette in order.
- **Allowed colors** (Image > Palettes > Load Allowed Colors) limits the colors a palette may take to a list, such as
  what a machine can show; a color chosen outside it becomes the nearest allowed one.

The palettes are saved apart, as a palette setup in JSON (Image > Palettes > Save Palette Setup); one palette can be
loaded or saved alone as .pal, .gpl or .act, or taken from an image's palette.

Opening a PNG that has a palette asks whether to keep it indexed or open it in colors. Saving an indexed image as PNG
asks whether to save it indexed, with every palette one after the other, or in colors. Image > Convert to Indexed and
Convert to Color change an open image from one to the other.

## Files

| Format | Opens | Saves |
|---|---|---|
| OpenRaster (.ora) | with its layers | with its layers |
| PNG | 8 and 16 bits, and with a palette | 8 and 16 bits, and indexed |
| JPEG, BMP, TGA | yes | yes, at 8 bits |
| GIF | yes | no |
| Raw 16-bit heightmap (.r16, .raw) | yes, asking its size | yes |

Copy and paste work with other programs both ways. File > Open Recent lists the last files opened or saved.

## Effects

Adjustments and Effects run on the selection, or on the whole layer when nothing is selected; their dialogs show the
change as the values move.

- **Curves** (Ctrl+Shift+M) draws what each level becomes over the image's histogram: a click adds a point, a drag
  moves it, a right click or dragging it far off takes it away. The transfer map is luminosity, which keeps the
  colours' hues, or red, green and blue, the ones ticked edited together.
- **Levels** (Ctrl+L) has the histogram of what goes in, with markers for black, grey and white, and what comes out
  under it; Auto stretches what the image has from black to white. Red, green and blue ticked are edited together,
  each keeping levels of its own, as with all three ticked they share them.
 Ctrl+F repeats the last effect. Every effect is a plugin, and more can be added:
Effects > Manage Plugins writes and tries them beside an image; see [Plugins](plugins.md).

## Settings

OUI Paint keeps its settings in `oui_paint.ini`: beside the program when its folder can be written to, in the user's
configuration folder when not. The `plugins` folder beside it holds the plugins added.
