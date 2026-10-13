// What a plugin of OUI Paint can use, as TypeScript declarations: the plugin workbench suggests from them as one
// writes, and the Plugins page of the help lists the images' methods from them. prelude.js is what makes it so; each
// declaration is on a line of its own, its description in the comment over it.

declare namespace paint {
    /**
     * Declares an effect, which appears in the Adjustments menu or in a submenu of Effects.
     */
    function effect(definition: EffectDefinition): void;
    /**
     * Calls `hook` with the path of each file saved, after it is saved.
     */
    function onSaved(hook: (path: string) => void): void;
    /**
     * Fractal noise from black to white, `scale` the size of its features, `roughness` from 0 to 1.
     */
    function clouds(width: number, height: number, scale: number, roughness: number, seed: number): Image;

    /**
     * An effect, as paint.effect takes it: `render` makes the whole image, or `row` makes it a row at a time.
     */
    class EffectDefinition {
        /**
         * Its name in the menu.
         */
        name: string;
        /**
         * "Adjustments", or the Effects submenu it goes in, as "Blurs".
         */
        menu: string;
        /**
         * What its dialog asks for: a slider and a field for each number, a list for each choice.
         */
        parameters: Parameter[];
        /**
         * The program's own dialog for it in place of a slider for each parameter: "curves" or "levels".
         */
        dialog: string;
        /**
         * With `row`: its rows are made on every core at once, each core with its own copy of the plugins, so `row`
         * reads only the row it is given.
         */
        parallel: boolean;
        /**
         * Makes the effect of the image, given the values of its parameters by id, and gives the result.
         */
        render(image: Image, values: Values, context: EffectContext): Image;
        /**
         * Makes one row, `y` of the image: red, green, blue and alpha from 0 to 1, colour multiplied by alpha,
         * changed in place or another array given back.
         */
        row(row: Float32Array, y: number, values: Values, context: EffectContext): Float32Array;
    }

    /**
     * What an effect asks for: a number between `min` and `max`, or a choice among `options`.
     */
    class Parameter {
        /**
         * Its name in `values`.
         */
        id: string;
        /**
         * Its name in the dialog.
         */
        name: string;
        min: number;
        max: number;
        /**
         * Where it starts; for a choice, the index of the one chosen.
         */
        value: number;
        /**
         * What every value is a whole number of: 1 unless given, 0 for any.
         */
        step: number;
        /**
         * The names to choose from; the value is then the index of the one chosen.
         */
        options: string[];
    }

    /**
     * The values of the effect's parameters, by their ids; the workbench declares those of the effect being written.
     */
    class Values {
    }

    /**
     * What an effect is told besides its parameters' values.
     */
    class EffectContext {
        /**
         * The primary color, as [red, green, blue, alpha] from 0 to 1.
         */
        primary: number[];
        /**
         * The secondary color, as [red, green, blue, alpha] from 0 to 1.
         */
        secondary: number[];
        /**
         * The bounds of the selection, the whole image when nothing is selected.
         */
        selection: Rectangle;
    }

    /**
     * A rectangle in pixels.
     */
    class Rectangle {
        x: number;
        y: number;
        width: number;
        height: number;
    }

    /**
     * Pixels kept in OUI Paint: red, green, blue and alpha from 0 to 1, colour multiplied by alpha, whatever the
     * image's depth. Its methods run in C++ and give new images, so chaining them copies nothing into JavaScript.
     */
    class Image {
        /**
         * Its width in pixels.
         */
        width: number;
        /**
         * Its height in pixels.
         */
        height: number;
        /**
         * Blurred as by a lens, `radius` pixels round each.
         */
        gaussianBlur(radius: number): Image;
        /**
         * Blurred evenly over a square `radius` pixels round each.
         */
        boxBlur(radius: number): Image;
        /**
         * Smeared along a line at `degrees`, `distance` pixels long.
         */
        motionBlur(degrees: number, distance: number): Image;
        /**
         * Smeared round a point, by `degrees`.
         */
        radialBlur(degrees: number, centerX: number, centerY: number): Image;
        /**
         * Smeared towards a point, `amount` from 0 to 1.
         */
        zoomBlur(amount: number, centerX: number, centerY: number): Image;
        /**
         * Each pixel its neighbours times a square matrix of odd size, row by row, divided and offset.
         */
        convolve(matrix: number[], divisor?: number, offset?: number): Image;
        /**
         * Each channel through a table of values from 0 to 1, interpolated; one table for all three when only one is
         * given.
         */
        lookup(red: number[], green?: number[], blue?: number[]): Image;
        /**
         * Each pixel's luminosity through a table of values from 0 to 1, interpolated, its colour scaled with it so
         * its hue stays.
         */
        lookupLuminosity(table: number[]): Image;
        /**
         * Hue turned by degrees, saturation and lightness changed by -1 to 1.
         */
        hueSaturation(hue: number, saturation: number, lightness: number): Image;
        /**
         * This image laid over `below` as a layer, by a blend mode's name: Normal, Multiply, Additive, Color Burn,
         * Color Dodge, Reflect, Glow, Overlay, Difference, Negation, Lighten, Darken, Screen or Xor.
         */
        over(below: Image, mode?: string, opacity?: number): Image;
        /**
         * Each row given to `change(row, y)` as a Float32Array, changed in place or replaced by the one it gives back.
         */
        rows(change: (row: Float32Array, y: number) => Float32Array): Image;
    }
}

declare namespace console {
    /**
     * Writes its arguments, separated by spaces, under the image in the workbench.
     */
    function log(...values: any[]): void;
}
