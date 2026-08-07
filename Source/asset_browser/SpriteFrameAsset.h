#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class SpriteFrameAsset final : public Asset
{
public:
    SpriteFrameAsset();

    const char* icon(bool open) const override;
    void drawTooltip() const override;
    void drawPreview(EditorContext& context, ImVec2 availableSize) const override;
    void fillDragPayload(AssetDragPayload& payload) const override;

    std::filesystem::path atlasPath;
    std::string frameName;
    int width = 0;
    int height = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
};
}  // namespace editor
