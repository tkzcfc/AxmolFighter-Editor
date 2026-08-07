#include "ui/EditorDockSpace.h"

#include "imgui.h"

namespace editor
{
namespace
{
constexpr const char* kDockSpaceName = "AxmolEditorDockSpace";
}  // namespace

ImGuiID EditorDockSpace::draw()
{
    return drawDockSpace();
}

ImGuiID EditorDockSpace::dockSpaceId()
{
    return ImGui::GetID(kDockSpaceName);
}

ImGuiID EditorDockSpace::drawDockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("Editor DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspaceId = dockSpaceId();
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    return dockspaceId;
}
}  // namespace editor
