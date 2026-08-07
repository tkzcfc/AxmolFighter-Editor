#include "scene/canvas/SceneCanvasTool.h"

#include "imgui.h"

namespace editor
{
const char* SceneCanvasTool::icon() const
{
    return displayName().data();
}

bool SceneCanvasTool::isAvailable(const SceneDocument&) const
{
    return true;
}

void SceneCanvasTool::drawToolbarButton(std::string& activeToolId) const
{
    const bool active = activeToolId == id();
    if (active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

    if (ImGui::SmallButton(icon()))
        activeToolId = std::string(id());

    if (active)
        ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", displayName().data());
}

void SceneCanvasTool::onActivate(EditorContext&, SceneDocument&) const {}

void SceneCanvasTool::onDeactivate(EditorContext&, SceneDocument&) const {}

bool SceneCanvasTool::handleInput(EditorContext&, SceneDocument&, const SceneCanvasContext&) const
{
    return false;
}

void SceneCanvasTool::drawOverlay(EditorContext&, SceneDocument&, const SceneCanvasContext&) const {}
}  // namespace editor
