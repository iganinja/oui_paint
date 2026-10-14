# Plugins

A plugin is a JavaScript file. OUI Paint loads its own built-in plugins and then every `.js` file in the `plugins`
folder beside its settings file (`oui_paint.ini`), in alphabetical order. A plugin declares effects, which appear in
the Adjustments or Effects menu, and hooks, which run after a file is saved.

## Writing one and trying it

Effects > Manage Plugins lists the plugins, built in and your own, with the effects each declares or why it failed to
load. **New** opens a tab to write one in, and **Edit** opens one; a built-in plugin opens as a new one, to save as your
own. The code is on the left; on the right, its effect on an image, made again a moment after each change: the test
card, an image open in another tab, or one loaded from a file (Try on). Under it are the effect's parameters, what the
plugin prints with `console.log`, and why it fails, with its line, which a click goes to. A run taking over three
seconds counts as stuck. As you write, it suggests what the code can use: after `image.` the methods of an image,
after `paint.` what `paint` has, and inside the brackets of a call how it is called and what it does; Ctrl+Space asks
for the suggestions where the cursor is.

Ctrl+S saves it in the plugins folder, and its effects are in the menus at once. **Reload** loads every plugin again
after changing them elsewhere.

## Effects

```js
paint.effect({
    name: "Soft Glow",
    menu: "Photo",                  // "Adjustments", or the Effects submenu it goes in
    parameters: [
        {id: "radius", name: "Radius", min: 0, max: 100, value: 6},
        {id: "strength", name: "Strength", min: 0, max: 100, value: 50},
        {id: "mode", name: "Mode", options: ["Screen", "Lighten"], value: 0},
    ],
    render(image, values, context) {
        const glow = image.gaussianBlur(values.radius);
        return glow.over(image, ["Screen", "Lighten"][values.mode], values.strength / 100);
    },
});
```

Each parameter becomes a slider and a number in the effect's dialog, or a list when it has `options` (its value is
then the index chosen). `step` sets what values are whole numbers of (1 unless given, 0 for any). The image changes
as the parameters do, and the effect is applied inside the selection only, as much as each pixel is selected.

`context` holds `primary` and `secondary`, the colours as `[red, green, blue, alpha]` from 0 to 1, and `selection`,
its bounds as `{x, y, width, height}`.

## Images

An image is a handle to pixels kept in C++; its methods run pieces written in C++ and give new images, so chaining
them costs no copying into JavaScript. Pixels are red, green, blue and alpha from 0 to 1, colour multiplied by alpha,
whatever the document's depth.

<!-- image-methods -->
The methods are declared, with what each does, in [paint.d.ts](../src/plugins/paint.d.ts), which the plugin
workbench suggests from as you write; the help inside OUI Paint lists them here.
<!-- /image-methods -->

`paint.clouds(width, height, scale, roughness, seed)` makes fractal noise from black to white.

## Row by row, on every core

When no piece fits, an effect can make its rows in JavaScript with `row` in place of `render`. With `parallel: true`,
its rows are made on every core at once: each core runs its own copy of the plugins, so `row` must read only the row
it is given, and the `oui` system functions are not there.

```js
paint.effect({
    name: "Scanlines",
    menu: "Render",
    parameters: [{id: "every", name: "Every", min: 2, max: 20, value: 4}],
    parallel: true,
    row(row, y, values) {
        if (y % values.every !== 0) return;
        for (let i = 0; i < row.length; i += 4) {
            row[i] = row[i + 1] = row[i + 2] = 0;
        }
    },
});
```

## Indexed images

Image > Mode > Indexed turns an image into indices of one palette by an indexer, which plugins declare with
`paint.indexer`. Its `palette` gives the palette, as a list of `[red, green, blue]` from 0 to 1, of no more than
`context.count` colours; when the user loads a palette to put the image in, `context.palette` holds it. Its `assign`,
when it has one, gives the image of indices, each pixel's red its index over 255, as `nearest`, `ditherFloydSteinberg`
and `ditherBayer` make it; without one, each pixel takes the nearest colour.

```js
paint.indexer({
    name: "Web safe",
    parameters: [{id: "dither", name: "Dithering", options: ["None", "Ordered 4x4"], value: 0}],
    palette(image, values, context) {
        if (context.palette) return context.palette;
        const steps = [0, 0.2, 0.4, 0.6, 0.8, 1];
        const palette = [];
        for (const r of steps) for (const g of steps) for (const b of steps) palette.push([r, g, b]);
        return palette;
    },
    assign(image, palette, values) {
        return values.dither === 1 ? image.ditherBayer(palette, 4, 1) : image.nearest(palette);
    },
});
```

All the layers are indexed together, as one image, so the palette fits them all. A pixel see-through, half or more,
takes index 0, kept see-through, whatever the palette; then there is room for 255 colours, else 256. OUI Paint comes
with Median cut, dithered or not, and Exact colors, the image's own colours when there are no more than asked.
`paint.medianCut(image, count)` and `image.colors(most)` are there for indexers of one's own.

## Changing the format

When Image > Mode loses something, colour to grey or 16 bits a channel to 8, it asks which converter to do it with,
which plugins declare with `paint.converter`. `to` says which change it does, `"grayscale"` or `"8 bits"`, and
`render` gives the image as the new format should keep it: grey in its three channels, or what 8 bits keep.

```js
paint.converter({
    name: "Desaturate",
    to: "grayscale",
    parameters: [],
    render(image) {
        return image.rows((row) => {
            for (let i = 0; i < row.length; i += 4) {
                const grey = (Math.max(row[i], row[i + 1], row[i + 2]) + Math.min(row[i], row[i + 1], row[i + 2])) / 2;
                row[i] = row[i + 1] = row[i + 2] = grey;
            }
        });
    },
});
```

From 16 bits colour to 8 bits grey goes through both, grey first.

## After saving

```js
paint.onSaved((path) => {
    if (path.endsWith(".png")) {
        oui.run(`optipng -quiet "${path}"`);
    }
});
```

Hooks get the path of the file saved. They have OUI's system functions as `oui`: `oui.readFile`, `oui.writeFile`,
`oui.exists`, `oui.listDir`, `await oui.run(command, {cwd, input, output})`, and `oui.message`, `oui.confirm`,
`oui.prompt` and `oui.choose` to ask.
