// What plugins see of OUI Paint: `paint` to declare effects and hooks, and images to work on. Every image is a
// handle to pixels kept in C++; its methods run pieces written in C++ and give new images.
"use strict";

const native = __paint;
const effects = [];
const indexers = [];
const converters = [];
// A palette as the pieces take it: three numbers a colour, one after the other.
const flat = (palette) => new Float32Array(palette.flatMap((color) => [color[0], color[1], color[2]]));
const saved = [];

class PaintImage {
    constructor(id) {
        this.id = id;
    }
    get width() {
        return native.width(this.id);
    }
    get height() {
        return native.height(this.id);
    }
    gaussianBlur(radius) {
        return new PaintImage(native.gaussianBlur(this.id, radius));
    }
    boxBlur(radius) {
        return new PaintImage(native.boxBlur(this.id, radius));
    }
    motionBlur(degrees, distance) {
        return new PaintImage(native.motionBlur(this.id, degrees, distance));
    }
    radialBlur(degrees, centerX, centerY) {
        return new PaintImage(native.radialBlur(this.id, degrees, centerX, centerY));
    }
    zoomBlur(amount, centerX, centerY) {
        return new PaintImage(native.zoomBlur(this.id, amount, centerX, centerY));
    }
    // A square matrix of numbers, row by row.
    convolve(matrix, divisor = 1, offset = 0) {
        return new PaintImage(native.convolve(this.id, new Float32Array(matrix), divisor, offset));
    }
    // A table from 0 to 1 for each colour channel, the same for all three when only one is given.
    lookup(red, green = red, blue = red) {
        return new PaintImage(
            native.lookup(this.id, new Float32Array(red), new Float32Array(green), new Float32Array(blue)));
    }
    // Each pixel's luminosity through a table from 0 to 1, its colour scaled with it so its hue stays.
    lookupLuminosity(table) {
        return new PaintImage(native.lookupLuminosity(this.id, new Float32Array(table)));
    }
    hueSaturation(hue, saturation, lightness) {
        return new PaintImage(native.hueSaturation(this.id, hue, saturation, lightness));
    }
    // This image laid over `below`, as a layer blend mode, by its name, mixes them.
    over(below, mode = "Normal", opacity = 1) {
        return new PaintImage(native.mix(below.id, this.id, mode, opacity));
    }
    // Grey: each pixel the weighted sum of its red, green and blue.
    grayscale(red = 0.299, green = 0.587, blue = 0.114) {
        return new PaintImage(native.grayscale(this.id, red, green, blue));
    }
    // Each channel brought to `levels` even steps, nudged by a Bayer matrix of `size` 2, 4 or 8 first, `strength`
    // from 0, plain rounding, to 1.
    ditherLevels(levels, size = 4, strength = 1) {
        return new PaintImage(native.ditherLevels(this.id, levels, size, strength));
    }
    // Its different colours, as {color: [red, green, blue], count}, the commonest first, no more than `most`.
    colors(most = 65536) {
        return JSON.parse(native.colors(this.id, most));
    }
    // An image of indices: each pixel the index of the palette's colour nearest it.
    nearest(palette) {
        return new PaintImage(native.nearest(this.id, flat(palette)));
    }
    // An image of indices dithered by Floyd and Steinberg's error diffusion, `strength` from 0 to 1.
    ditherFloydSteinberg(palette, strength = 1) {
        return new PaintImage(native.ditherFloydSteinberg(this.id, flat(palette), strength));
    }
    // An image of indices dithered by a Bayer matrix of `size` 2, 4 or 8, `strength` from 0 to 1.
    ditherBayer(palette, size = 4, strength = 1) {
        return new PaintImage(native.ditherBayer(this.id, flat(palette), size, strength));
    }
    // Each row as a Float32Array of red, green, blue and alpha from 0 to 1, colour multiplied by alpha, given to
    // `change(row, y)`, which changes it in place or returns another.
    rows(change) {
        const result = native.copy(this.id);
        for (let y = 0; y < this.height; y++) {
            const row = native.row(result, y);
            const changed = change(row, y);
            native.setRow(result, y, changed instanceof Float32Array ? changed : row);
        }
        return new PaintImage(result);
    }
}

globalThis.paint = {
    // Declares an effect: {name, menu, parameters, render(image, values, context)}, or with row(row, y, values,
    // context) in place of render, made a row at a time; with parallel: true, those rows are made on every core at
    // once, each core with its own copy of the plugins, without the `oui` system functions.
    effect(spec) {
        effects.push(spec);
        native.register(spec);
    },
    // Calls `hook(path)` after a file is saved.
    onSaved(hook) {
        saved.push(hook);
    },
    // Declares a way of turning an image into indices of a palette: {name, parameters, palette(image, values, context),
    // assign(image, palette, values, context)}, assign giving an image of indices, the nearest colours when left out.
    indexer(spec) {
        indexers.push(spec);
        native.registerIndexer(spec);
    },
    // Declares a way of changing an image's format: {name, to: "grayscale" or "8 bits", parameters,
    // render(image, values)}.
    converter(spec) {
        converters.push(spec);
        native.registerConverter(spec);
    },
    // A palette of up to `count` colours for the image by median cut.
    medianCut(image, count) {
        return JSON.parse(native.medianCut(image.id, count));
    },
    clouds(width, height, scale, roughness, seed) {
        return new PaintImage(native.clouds(width, height, scale, roughness, seed));
    },
};

native.setRunner((index, id, values, context) => {
    const result = effects[index].render(new PaintImage(id), JSON.parse(values), JSON.parse(context));
    return result.id;
});
native.setRowRunner((index, id, values, context) => {
    const effect = effects[index];
    const parsed = JSON.parse(values);
    const told = JSON.parse(context);
    const height = native.height(id);
    for (let y = 0; y < height; y++) {
        const row = native.row(id, y);
        const changed = effect.row(row, told.firstRow + y, parsed, told);
        native.setRow(id, y, changed instanceof Float32Array ? changed : row);
    }
    return id;
});
native.setIndexRunner((index, id, values, context) => {
    const indexer = indexers[index];
    const image = new PaintImage(id);
    const parsed = JSON.parse(values);
    const told = JSON.parse(context);
    const palette = indexer.palette(image, parsed, told);
    const indices = indexer.assign ? indexer.assign(image, palette, parsed, told) : image.nearest(palette);
    return JSON.stringify({palette: palette.map((color) => [color[0], color[1], color[2]]), indices: indices.id});
});
native.setConvertRunner((index, id, values) => {
    return converters[index].render(new PaintImage(id), JSON.parse(values)).id;
});
native.setSaved((path) => {
    for (const hook of saved) {
        hook(path);
    }
});
