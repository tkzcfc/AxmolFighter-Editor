#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace editor::npk
{

// Kept as a small editor-local metadata type for code that consumes the
// client-side NPK resource description. The exporter itself uses TextureInfo.
struct NpkImgTexture
{
    uint16_t width        = 0;
    uint16_t height       = 0;
    uint16_t x            = 0;
    uint16_t y            = 0;
    uint16_t canvasWidth  = 0;
    uint16_t canvasHeight = 0;
    int16_t linkIndex     = -1;
    uint32_t length       = 0;
    uint64_t dataOffset   = 0;
    uint32_t dataLength   = 0;
};

struct NpkImg
{
    std::string name;
    std::vector<NpkImgTexture> textures;
};

}  // namespace editor::npk
