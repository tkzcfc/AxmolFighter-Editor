#pragma once

#include <cstdint>

namespace editor
{
enum class AssetKind : std::uint8_t
{
    Unknown,
    Folder,
    Texture,
    SpriteAtlas,
    SpriteFrame,
    BitmapFont,
    TrueTypeFont,
    Spine,
    Layer,
    FrameAnimation,
    Box,
    Motion,
    Text
};
}  // namespace editor
