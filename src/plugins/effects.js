// Paint.NET's effects, built on OUI Paint's pieces.
"use strict";

paint.effect({
    name: "Gaussian Blur",
    menu: "Blurs",
    parameters: [{id: "radius", name: "Radius", min: 0, max: 200, value: 2}],
    render(image, values) {
        return image.gaussianBlur(values.radius);
    },
});

paint.effect({
    name: "Motion Blur",
    menu: "Blurs",
    parameters: [
        {id: "angle", name: "Angle", min: -180, max: 180, value: 25},
        {id: "distance", name: "Distance", min: 1, max: 200, value: 10},
    ],
    render(image, values) {
        return image.motionBlur(values.angle, values.distance);
    },
});

paint.effect({
    name: "Radial Blur",
    menu: "Blurs",
    parameters: [
        {id: "angle", name: "Angle", min: 0, max: 360, value: 2},
        {id: "x", name: "Center X", min: -100, max: 100, value: 0},
        {id: "y", name: "Center Y", min: -100, max: 100, value: 0},
    ],
    render(image, values) {
        const x = image.width * (0.5 + values.x / 200);
        const y = image.height * (0.5 + values.y / 200);
        return image.radialBlur(values.angle, x, y);
    },
});

paint.effect({
    name: "Zoom Blur",
    menu: "Blurs",
    parameters: [
        {id: "amount", name: "Zoom amount", min: 0, max: 100, value: 10},
        {id: "x", name: "Center X", min: -100, max: 100, value: 0},
        {id: "y", name: "Center Y", min: -100, max: 100, value: 0},
    ],
    render(image, values) {
        const x = image.width * (0.5 + values.x / 200);
        const y = image.height * (0.5 + values.y / 200);
        return image.zoomBlur(values.amount / 100, x, y);
    },
});

// A 3 by 3 matrix that weighs the neighbours on one side against those on the other, at `degrees`.
function directional(degrees, strength) {
    const radians = (degrees * Math.PI) / 180;
    const dx = Math.cos(radians);
    const dy = -Math.sin(radians);
    const matrix = [];
    for (let j = -1; j <= 1; j++) {
        for (let i = -1; i <= 1; i++) {
            matrix.push((i * dx + j * dy) * strength);
        }
    }
    return matrix;
}

paint.effect({
    name: "Edge Detect",
    menu: "Stylize",
    parameters: [{id: "angle", name: "Angle", min: -180, max: 180, value: 45}],
    render(image, values) {
        return image.convolve(directional(values.angle, 1), 1, 0.5);
    },
});

paint.effect({
    name: "Emboss",
    menu: "Stylize",
    parameters: [{id: "angle", name: "Angle", min: -180, max: 180, value: 0}],
    render(image, values) {
        // Grey relief: the edges alone, on the middle grey.
        return image.hueSaturation(0, -1, 0).convolve(directional(values.angle, 2), 1, 0.5);
    },
});

paint.effect({
    name: "Relief",
    menu: "Stylize",
    parameters: [{id: "angle", name: "Angle", min: -180, max: 180, value: 45}],
    render(image, values) {
        // The image itself, with its edges lit from the angle.
        const matrix = directional(values.angle, 1);
        matrix[4] += 1;
        return image.convolve(matrix, 1, 0);
    },
});

paint.effect({
    name: "Sharpen",
    menu: "Photo",
    parameters: [{id: "amount", name: "Amount", min: 1, max: 20, value: 10}],
    render(image, values) {
        const a = values.amount / 20;
        return image.convolve([0, -a, 0, -a, 1 + 4 * a, -a, 0, -a, 0]);
    },
});

paint.effect({
    name: "Add Noise",
    menu: "Noise",
    parameters: [
        {id: "intensity", name: "Intensity", min: 0, max: 100, value: 64},
        {id: "saturation", name: "Color saturation", min: 0, max: 400, value: 100},
        {id: "coverage", name: "Coverage", min: 0, max: 100, value: 100},
    ],
    render(image, values) {
        // Written pixel by pixel in JavaScript, as an example of what a plugin can do when no piece fits.
        const intensity = values.intensity / 100 / 2;
        const color = values.saturation / 100;
        const coverage = values.coverage / 100;
        return image.rows((row) => {
            for (let i = 0; i < row.length; i += 4) {
                if (Math.random() > coverage) continue;
                const alpha = row[i + 3];
                const gray = (Math.random() * 2 - 1) * intensity;
                for (let c = 0; c < 3; c++) {
                    const noise = gray + (Math.random() * 2 - 1) * intensity * color * 0.5;
                    row[i + c] = Math.min(alpha, Math.max(0, row[i + c] + noise * alpha));
                }
            }
        });
    },
});

paint.effect({
    name: "Clouds",
    menu: "Render",
    parameters: [
        {id: "scale", name: "Scale", min: 2, max: 1000, value: 250},
        {id: "power", name: "Power", min: 0, max: 100, value: 50},
        {id: "seed", name: "Seed", min: 0, max: 99999, value: 1},
        {
            id: "mode",
            name: "Blend mode",
            options: ["Normal", "Multiply", "Additive", "Color Burn", "Color Dodge", "Reflect", "Glow", "Overlay",
                "Difference", "Negation", "Lighten", "Darken", "Screen", "Xor"],
            value: 0,
        },
    ],
    render(image, values, context) {
        const modes = ["Normal", "Multiply", "Additive", "Color Burn", "Color Dodge", "Reflect", "Glow", "Overlay",
            "Difference", "Negation", "Lighten", "Darken", "Screen", "Xor"];
        // From the primary colour where the noise is dark to the secondary where it is light.
        const from = context.primary;
        const to = context.secondary;
        const channel = (c) => [from[c], to[c]];
        const clouds = paint.clouds(image.width, image.height, values.scale, values.power / 100, values.seed)
            .lookup(channel(0), channel(1), channel(2));
        return clouds.over(image, modes[values.mode]);
    },
});
