#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class TextureAsset final : public Asset
{
public:
    TextureAsset();

    const char* icon(bool open) const override;
    void drawTooltip() const override;
    void drawPreview(EditorContext& context, ImVec2 availableSize) const override;
    void fillDragPayload(AssetDragPayload& payload) const override;
};
}  // namespace editor
