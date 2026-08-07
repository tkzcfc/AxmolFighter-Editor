#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class MotionAsset final : public Asset
{
public:
    MotionAsset();
    const char* icon(bool open) const override;
    void drawTooltip() const override;
    void drawPreview(EditorContext& context, ImVec2 availableSize) const override;
    bool canOpenDocument() const override;
};
}  // namespace editor
