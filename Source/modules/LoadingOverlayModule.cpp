#include "modules/LoadingOverlayModule.h"

#include "asset_browser/AssetBrowserService.h"
#include "core/EditorContext.h"
#include "imgui.h"

#include <cstdio>

namespace editor
{
void LoadingOverlayModule::onAttach(EditorContext&) {}

void LoadingOverlayModule::onDetach(EditorContext&) {}

void LoadingOverlayModule::onUpdate(EditorContext&, float) {}

void LoadingOverlayModule::onImGuiRender(EditorContext& context)
{
    auto browser = context.services().get<AssetBrowserService>();
    if (!browser || !browser->isScanning())
        return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    constexpr ImGuiWindowFlags overlayFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav;

    ImGui::Begin("##GlobalLoadingOverlay", nullptr, overlayFlags);

    const std::size_t completed = browser->scanCompleted();
    const std::size_t total = browser->scanTotal();
    const float fraction = total == 0 ? 0.0f : static_cast<float>(completed) / static_cast<float>(total);
    char overlay[64];
    if (total == 0)
        overlay[0] = '\0';
    else
        std::snprintf(overlay, sizeof(overlay), "%zu / %zu", completed, total);

    constexpr float cardWidth = 320.0f;
    constexpr float cardHeight = 64.0f;
    const ImVec2 windowSize = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2((windowSize.x - cardWidth) * 0.5f, (windowSize.y - cardHeight) * 0.5f));
    if (ImGui::BeginChild("##LoadingCard", ImVec2(cardWidth, cardHeight), true))
    {
        ImGui::TextUnformatted("Loading");
        ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay[0] == '\0' ? nullptr : overlay);
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}
}  // namespace editor
