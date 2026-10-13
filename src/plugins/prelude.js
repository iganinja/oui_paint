// What plugins see of OUI Paint: `paint` to declare effects and hooks, and images to work on. Every image is a
// handle to pixels kept in C++; its methods run pieces written in C++ and give new images.
"use strict";

const native = __paint;
const effects = [];
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
    hueSaturation(hue, saturation, lightness) {
        return new PaintImage(native.hueSaturation(this.id, hue, saturation, lightness));
    }
    // This image laid over `below`, as a layer blend mode, by its name, mixes them.
    over(below, mode = "Normal", opacity = 1) {
        return new PaintImage(native.mix(below.id, this.id, mode, opacity));
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
native.setSaved((path) => {
    for (const hook of saved) {
        hook(path);
    }
});
