#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class AniAsset final : public Asset
{
public:
    AniAsset();
    const char* icon(bool open) const override;
    void drawTooltip() const override;
    void drawPreview(EditorContext& context, ImVec2 availableSize) const override;
    bool canOpenDocument() const override;
};
}  // namespace editor
