#pragma once

#include "core/document.h"

namespace oui_paint
{

enum class Background
{
    white,
    black,
    transparent,
};

// What a new image is made of, as the New Image dialog asks.
struct NewImage
{
    int width{800};
    int height{600};
    int bits{8};
    bool gray{false};
    // Indexed, with `palettes` palettes of `palette_size` greys to start with.
    bool indexed{false};
    int palettes{4};
    int palette_size{16};
    Background background{Background::white};
};

// A document of one layer, "Background", filled with the background asked for.
[[nodiscard]] Document make_document(const NewImage& request);

} // namespace oui_paint
