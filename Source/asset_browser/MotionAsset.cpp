#include "asset_browser/MotionAsset.h"

#include "documents/MotionDocument.h"

namespace editor
{
namespace
{
constexpr const char* kIconMotion = "\xef\x81\x8d";  // fa-exchange
}

MotionAsset::MotionAsset() : Asset(AssetKind::Motion) {}

const char* MotionAsset::icon(bool) const { return kIconMotion; }

void MotionAsset::drawTooltip() const
{
    MotionDocument document;
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    drawAssetBaseProperties(*this);
    if (document.open(absolutePath))
    {
        ImGui::Text("Motions: %zu", document.data().motions.size());
        std::size_t animationCount = 0;
        for (const MotionEntry& motion : document.data().motions)
            animationCount += motion.animations.size();
        ImGui::Text("Animations: %zu", animationCount);
    }
    else
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", document.lastError().c_str());
    }
    ImGui::EndTooltip();
}

void MotionAsset::drawPreview(EditorContext&, ImVec2) const
{
    MotionDocument document;
    if (!document.open(absolutePath))
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", document.lastError().c_str());
        return;
    }
    ImGui::TextUnformatted("Motion mapping");
    ImGui::Separator();
    ImGui::Text("Motions: %zu", document.data().motions.size());
    std::size_t animationCount = 0;
    for (const MotionEntry& motion : document.data().motions)
        animationCount += motion.animations.size();
    ImGui::Text("Animations: %zu", animationCount);
}

bool MotionAsset::canOpenDocument() const { return true; }
}  // namespace editor
