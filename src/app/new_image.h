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
    Background background{Background::white};
};

// A document of one layer, "Background", filled with the background asked for.
[[nodiscard]] Document make_document(const NewImage& request);

} // namespace oui_paint
