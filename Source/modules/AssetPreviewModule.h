#pragma once

#include "modules/PanelModule.h"

#include <filesystem>

namespace editor
{
class AssetPreviewModule final : public PanelModule
{
public:
    AssetPreviewModule();

private:
    void drawContent(EditorContext& context) override;
    std::filesystem::path assetRoot(EditorContext& context) const;
};
}  // namespace editor
