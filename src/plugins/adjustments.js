// Paint.NET's adjustments, built on OUI Paint's pieces.
"use strict";

// A table of `size` entries, each `f` of its place from 0 to 1.
function table(f, size = 256) {
    const result = [];
    for (let i = 0; i < size; i++) {
        result.push(Math.min(1, Math.max(0, f(i / (size - 1)))));
    }
    return result;
}

paint.effect({
    name: "Auto-Level",
    menu: "Adjustments",
    parameters: [],
    render(image) {
        // Each channel stretched from its darkest to its lightest.
        const low = [1, 1, 1];
        const high = [0, 0, 0];
        image.rows((row) => {
            for (let i = 0; i < row.length; i += 4) {
                const alpha = row[i + 3];
                if (alpha <= 0) continue;
                for (let c = 0; c < 3; c++) {
                    const value = row[i + c] / alpha;
                    if (value < low[c]) low[c] = value;
                    if (value > high[c]) high[c] = value;
                }
            }
        });
        const stretch = (c) => table((x) => (high[c] > low[c] ? (x - low[c]) / (high[c] - low[c]) : x));
        return image.lookup(stretch(0), stretch(1), stretch(2));
    },
});

paint.effect({
    name: "Black and White",
    menu: "Adjustments",
    parameters: [],
    render(image) {
        return image.hueSaturation(0, -1, 0);
    },
});

paint.effect({
    name: "Brightness / Contrast",
    menu: "Adjustments",
    parameters: [
        {id: "brightness", name: "Brightness", min: -100, max: 100, value: 0},
        {id: "contrast", name: "Contrast", min: -100, max: 100, value: 0},
    ],
    render(image, values) {
        const brightness = values.brightness / 100;
        const contrast = values.contrast >= 0 ? 1 / (1 - values.contrast / 100.0001) : 1 + values.contrast / 100;
        return image.lookup(table((x) => (x - 0.5) * contrast + 0.5 + brightness));
    },
});

paint.effect({
    name: "Hue / Saturation",
    menu: "Adjustments",
    parameters: [
        {id: "hue", name: "Hue", min: -180, max: 180, value: 0},
        {id: "saturation", name: "Saturation", min: 0, max: 200, value: 100},
        {id: "lightness", name: "Lightness", min: -100, max: 100, value: 0},
    ],
    render(image, values) {
        return image.hueSaturation(values.hue, values.saturation / 100 - 1, values.lightness / 100);
    },
});

paint.effect({
    name: "Invert Colors",
    menu: "Adjustments",
    parameters: [],
    render(image) {
        return image.lookup([1, 0]);
    },
});

paint.effect({
    name: "Invert Alpha",
    menu: "Adjustments",
    parameters: [],
    parallel: true,
    row(row) {
        for (let i = 0; i < row.length; i += 4) {
            const alpha = row[i + 3];
            const inverted = 1 - alpha;
            // Colour is kept multiplied by alpha: it is the same colour at the new alpha.
            const scale = alpha > 0 ? inverted / alpha : 0;
            row[i] *= scale;
            row[i + 1] *= scale;
            row[i + 2] *= scale;
            row[i + 3] = inverted;
        }
    },
});

paint.effect({
    name: "Levels",
    menu: "Adjustments",
    parameters: [
        {id: "inputLow", name: "Input black", min: 0, max: 255, value: 0},
        {id: "inputHigh", name: "Input white", min: 0, max: 255, value: 255},
        {id: "gamma", name: "Gamma", min: 0.1, max: 10, value: 1, step: 0},
        {id: "outputLow", name: "Output black", min: 0, max: 255, value: 0},
        {id: "outputHigh", name: "Output white", min: 0, max: 255, value: 255},
    ],
    render(image, values) {
        const inLow = values.inputLow / 255;
        const inHigh = Math.max(inLow + 1 / 255, values.inputHigh / 255);
        const outLow = values.outputLow / 255;
        const outHigh = values.outputHigh / 255;
        return image.lookup(table((x) => {
            const stretched = Math.min(1, Math.max(0, (x - inLow) / (inHigh - inLow)));
            return outLow + Math.pow(stretched, 1 / values.gamma) * (outHigh - outLow);
        }, 1024));
    },
});

paint.effect({
    name: "Posterize",
    menu: "Adjustments",
    parameters: [{id: "levels", name: "Levels", min: 2, max: 64, value: 16}],
    render(image, values) {
        const steps = values.levels - 1;
        return image.lookup(table((x) => Math.round(x * steps) / steps, 4096));
    },
});

paint.effect({
    name: "Sepia",
    menu: "Adjustments",
    parameters: [],
    render(image) {
        // Grey first, then tinted brown in its middle tones.
        return image.hueSaturation(0, -1, 0).lookup(
            table((x) => x * 1.07),
            table((x) => x * 0.95),
            table((x) => x * 0.77));
    },
});
