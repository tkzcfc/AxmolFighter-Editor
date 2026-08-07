#include "asset_browser/BoxAsset.h"

#include "documents/BoxDocument.h"

namespace editor
{
namespace
{
constexpr const char* kIconBox = "\xef\x8c\xa6";  // fa-cube
}

BoxAsset::BoxAsset() : Asset(AssetKind::Box) {}

const char* BoxAsset::icon(bool) const { return kIconBox; }

void BoxAsset::drawTooltip() const
{
    BoxDocument document;
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    drawAssetBaseProperties(*this);
    if (document.open(absolutePath))
    {
        ImGui::Text("Duration: %d ms", document.data().duration);
        ImGui::Text("Tracks: %zu", document.data().tracks.size());
        ImGui::Text("Events: %zu", document.data().events.size());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", document.lastError().c_str());
    }
    ImGui::EndTooltip();
}

void BoxAsset::drawPreview(EditorContext&, ImVec2) const
{
    BoxDocument document;
    if (!document.open(absolutePath))
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", document.lastError().c_str());
        return;
    }
    ImGui::TextUnformatted("Combat box data");
    ImGui::Separator();
    ImGui::Text("Duration: %d ms", document.data().duration);
    ImGui::Text("Tracks: %zu", document.data().tracks.size());
    ImGui::Text("Events: %zu", document.data().events.size());
}

bool BoxAsset::canOpenDocument() const { return true; }
}  // namespace editor
