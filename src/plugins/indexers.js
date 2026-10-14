// The ways OUI Paint turns an image into indices of a palette, Image > Mode > Indexed, and changes its format,
// Image > Mode, as any plugin can declare more.

// As many colours as there is room for, chosen by median cut, or a palette given; dithered or not.
paint.indexer({
    name: "Median cut",
    parameters: [
        {id: "dither", name: "Dithering", options: ["None", "Floyd-Steinberg", "Ordered 2x2", "Ordered 4x4",
            "Ordered 8x8"], value: 0},
        {id: "strength", name: "Strength", min: 0, max: 100, value: 100},
    ],
    palette(image, values, context) {
        return context.palette || paint.medianCut(image, context.count);
    },
    assign(image, palette, values) {
        const strength = values.strength / 100;
        switch (values.dither) {
        case 1:
            return image.ditherFloydSteinberg(palette, strength);
        case 2:
            return image.ditherBayer(palette, 2, strength);
        case 3:
            return image.ditherBayer(palette, 4, strength);
        case 4:
            return image.ditherBayer(palette, 8, strength);
        default:
            return image.nearest(palette);
        }
    },
});

// The colours the image has, when there is room for them all; into a palette given, the nearest of its colours.
paint.indexer({
    name: "Exact colors",
    parameters: [],
    palette(image, values, context) {
        if (context.palette) {
            return context.palette;
        }
        const found = image.colors(context.count + 1);
        if (found.length > context.count) {
            throw new Error(`The image has more than ${context.count} colors; Median cut chooses that many.`);
        }
        return found.map((entry) => entry.color);
    },
});

// Colour to grey, by how much each of red, green and blue counts.
paint.converter({
    name: "Luminosity",
    to: "grayscale",
    parameters: [{id: "weights", name: "Weights", options: ["Rec. 601 (classic video)", "Rec. 709 (HDTV, sRGB)",
        "Average", "Red only", "Green only", "Blue only"], value: 0}],
    render(image, values) {
        const weights = [[0.299, 0.587, 0.114], [0.2126, 0.7152, 0.0722], [1 / 3, 1 / 3, 1 / 3], [1, 0, 0], [0, 1, 0],
            [0, 0, 1]][values.weights];
        return image.grayscale(weights[0], weights[1], weights[2]);
    },
});

// 16 bits a channel to 8: rounded to the nearest, or dithered so smooth gradients keep no bands.
paint.converter({
    name: "Rounding",
    to: "8 bits",
    parameters: [{id: "dither", name: "Smooth gradients", options: ["No, to the nearest", "Yes, dithered"], value: 0}],
    render(image, values) {
        return values.dither === 1 ? image.ditherLevels(256, 4, 1) : image;
    },
});
