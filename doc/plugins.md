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
seconds counts as stuck.

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

| Method | What it gives |
|---|---|
| `width`, `height` | its size |
| `gaussianBlur(radius)`, `boxBlur(radius)` | blurred |
| `motionBlur(degrees, distance)` | smeared along a line |
| `radialBlur(degrees, centerX, centerY)` | smeared round a point |
| `zoomBlur(amount, centerX, centerY)` | smeared towards a point, `amount` from 0 to 1 |
| `convolve(matrix, divisor = 1, offset = 0)` | each pixel its neighbours times a square matrix of odd size |
| `lookup(red, green = red, blue = red)` | each channel through a table of values from 0 to 1, interpolated |
| `hueSaturation(hue, saturation, lightness)` | hue turned by degrees, saturation and lightness changed by -1 to 1 |
| `over(below, mode = "Normal", opacity = 1)` | this laid over `below` as a layer blend mode by its name |
| `rows(change)` | each row given to `change(row, y)` as a Float32Array, changed in place or replaced |

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
