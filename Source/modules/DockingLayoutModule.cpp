#include "modules/DockingLayoutModule.h"

#include "core/EditorContext.h"
#include "core/LayoutService.h"
#include "document_editors/DocumentEditorRegistry.h"
#include "imgui_internal.h"
#include "modules/LogModule.h"

namespace editor
{
void DockingLayoutModule::onAttach(EditorContext&) {}

void DockingLayoutModule::onDetach(EditorContext&) {}

void DockingLayoutModule::onUpdate(EditorContext&, float) {}

void DockingLayoutModule::onImGuiRender(EditorContext& context)
{
    const ImGuiID dockSpaceId = m_dockSpace.draw();
    if (shouldBuildLayout(context))
    {
        buildDefaultLayout(context, dockSpaceId);
        m_layoutInitialized = true;
    }
}

void DockingLayoutModule::buildDefaultLayout(EditorContext& context, ImGuiID dockSpaceId)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockSpaceId);
    ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodePos(dockSpaceId, viewport->WorkPos);
    ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize);

    ImGuiID centerNode = dockSpaceId;
    ImGuiID leftNode = 0;
    ImGuiID assetPreviewNode = 0;
    ImGuiID rightNode = 0;
    ImGuiID bottomNode = 0;

    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Left, 0.22f, &leftNode, &centerNode);
    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, 0.25f, &rightNode, &centerNode);
    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, 0.28f, &bottomNode, &centerNode);
    ImGui::DockBuilderSplitNode(leftNode, ImGuiDir_Down, 0.35f, &assetPreviewNode, &leftNode);

    ImGui::DockBuilderDockWindow("Assets", leftNode);
    ImGui::DockBuilderDockWindow("Asset Preview", assetPreviewNode);
    ImGui::DockBuilderDockWindow("Shape List", bottomNode);
    ImGui::DockBuilderDockWindow(LogModule::PanelId, bottomNode);
    if (const auto registry = context.services().get<DocumentEditorRegistry>())
    {
        for (const DocumentBottomPanelDescriptor& panel : registry->bottomPanels())
            ImGui::DockBuilderDockWindow(panel.title.c_str(), bottomNode);
    }
    ImGui::DockBuilderDockWindow("Scene Hierarchy", bottomNode);
    ImGui::DockBuilderDockWindow("Content", centerNode);
    ImGui::DockBuilderDockWindow("Inspector", rightNode);

    ImGui::DockBuilderFinish(dockSpaceId);
}

bool DockingLayoutModule::shouldBuildLayout(EditorContext& context) const
{
    const auto layoutService = context.services().get<LayoutService>();
    const bool resetRequested = layoutService && layoutService->consumeResetLayoutRequest();
    return !m_layoutInitialized || resetRequested;
}
}  // namespace editor
