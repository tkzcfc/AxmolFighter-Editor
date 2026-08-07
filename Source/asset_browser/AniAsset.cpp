#include "asset_browser/AniAsset.h"

#include "documents/AniDocument.h"

#include <algorithm>
#include <cstdint>

namespace editor
{
namespace
{
constexpr const char* kIconAnimation = "\xef\x88\xad";  // fa-film
}

AniAsset::AniAsset() : Asset(AssetKind::FrameAnimation) {}

const char* AniAsset::icon(bool) const
{
    return kIconAnimation;
}

void AniAsset::drawTooltip() const
{
    AniDocument document;
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    drawAssetBaseProperties(*this);
    if (document.open(absolutePath))
    {
        std::int64_t durationMs = 0;
        for (const AnimationFrameData& frame : document.animation().frames)
            durationMs += std::max(1, frame.delayMs);
        ImGui::Text("Frames: %zu", document.animation().frames.size());
        ImGui::Text("Duration: %lld ms (%.3f s)", static_cast<long long>(durationMs),
                    static_cast<double>(durationMs) / 1000.0);
    }
    else
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", document.lastError().c_str());
    }
    ImGui::EndTooltip();
}

void AniAsset::drawPreview(EditorContext&, ImVec2) const
{
    AniDocument document;
    if (!document.open(absolutePath))
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", document.lastError().c_str());
        return;
    }
    std::int64_t durationMs = 0;
    for (const AnimationFrameData& frame : document.animation().frames)
        durationMs += std::max(1, frame.delayMs);
    ImGui::Text("Frame animation");
    ImGui::Separator();
    ImGui::Text("Frames: %zu", document.animation().frames.size());
    ImGui::Text("Duration: %lld ms (%.3f seconds)", static_cast<long long>(durationMs),
                static_cast<double>(durationMs) / 1000.0);
    ImGui::Text("Loop: %s", document.animation().loop ? "Yes" : "No");
}

bool AniAsset::canOpenDocument() const
{
    return true;
}
}  // namespace editor
