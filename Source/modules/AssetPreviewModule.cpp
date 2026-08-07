#include "modules/AssetPreviewModule.h"

#include "asset_browser/Asset.h"
#include "asset_browser/AssetBrowserService.h"
#include "asset_browser/AssetSelectionService.h"
#include "core/EditorContext.h"
#include "core/ProjectSettings.h"
#include "imgui.h"

namespace editor
{
AssetPreviewModule::AssetPreviewModule() : PanelModule("Asset Preview") {}

void AssetPreviewModule::drawContent(EditorContext& context)
{
    const auto selection = context.services().get<AssetSelectionService>();
    const auto browser = context.services().get<AssetBrowserService>();
    if (!selection || !browser)
    {
        ImGui::TextDisabled("Asset preview service is not available.");
        return;
    }

    if (!selection->hasSelection())
    {
        ImGui::TextDisabled("No asset selected.");
        return;
    }

    const std::filesystem::path root = assetRoot(context);
    browser->rootAsset(root);
    const Asset* asset = browser->findAsset(selection->selectedAssetPath());
    if (!asset)
    {
        ImGui::TextDisabled("Selected asset is no longer available.");
        ImGui::TextWrapped("%s", selection->selectedAssetPath().generic_string().c_str());
        return;
    }

    ImGui::TextUnformatted(asset->displayName.c_str());
    ImGui::TextDisabled("%s", asset->relativePath.generic_string().c_str());
    ImGui::Separator();
    asset->drawPreview(context, ImGui::GetContentRegionAvail());
}

std::filesystem::path AssetPreviewModule::assetRoot(EditorContext& context) const
{
    return std::filesystem::path(context.settings().resourceRoot()).lexically_normal();
}
}  // namespace editor
