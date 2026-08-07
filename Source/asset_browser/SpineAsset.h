#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class SpineAsset final : public Asset
{
public:
    SpineAsset();

    const char* icon(bool open) const override;
    void fillDragPayload(AssetDragPayload& payload) const override;

    std::filesystem::path atlasPath;
};
}  // namespace editor
