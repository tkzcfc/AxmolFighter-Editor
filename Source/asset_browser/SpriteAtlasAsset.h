#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class SpriteAtlasAsset final : public Asset
{
public:
    SpriteAtlasAsset();

    const char* icon(bool open) const override;

    std::filesystem::path atlasPath;
};
}  // namespace editor
