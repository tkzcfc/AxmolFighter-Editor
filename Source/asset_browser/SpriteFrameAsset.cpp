#include "asset_browser/SpriteFrameAsset.h"

#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"
#include "ImGui/ImGuiPresenter.h"

#include <cstdio>

namespace editor
{
namespace
{
constexpr const char* kIconImage = "\xef\x87\x85";  // fa-file-image
}

SpriteFrameAsset::SpriteFrameAsset() : Asset(AssetKind::SpriteFrame) {}

const char* SpriteFrameAsset::icon(bool) const
{
    return kIconImage;
}

void SpriteFrameAsset::drawTooltip() const
{
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    ImGui::Text("Atlas: %s", atlasPath.generic_string().c_str());
    ImGui::Text("Frame: %s", frameName.c_str());
    if (width > 0 && height > 0)
        ImGui::Text("Frame: %d x %d px", width, height);
    if (sourceWidth > 0 && sourceHeight > 0)
        ImGui::Text("Source: %d x %d px", sourceWidth, sourceHeight);
    ImGui::EndTooltip();
}

void SpriteFrameAsset::drawPreview(EditorContext&, ImVec2 availableSize) const
{
    ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded(atlasPath.generic_string()))
        cache->addSpriteFramesWithFile(atlasPath.generic_string());

    ax::SpriteFrame* frame = cache->getSpriteFrameByName(frameName);
    if (!frame)
    {
        ImGui::TextDisabled("Unable to load sprite frame.");
        ImGui::TextWrapped("%s", frameName.c_str());
        return;
    }

    ax::extension::ImGuiPresenter::getInstance()->image(frame, availableSize, true);
}

void SpriteFrameAsset::fillDragPayload(AssetDragPayload& payload) const
{
    Asset::fillDragPayload(payload);
    const std::string atlas = atlasPath.generic_string();
    std::snprintf(payload.atlasPath, sizeof(payload.atlasPath), "%s", atlas.c_str());
    std::snprintf(payload.frameName, sizeof(payload.frameName), "%s", frameName.c_str());
    payload.width = width;
    payload.height = height;
    payload.sourceWidth = sourceWidth;
    payload.sourceHeight = sourceHeight;
}
}  // namespace editor
