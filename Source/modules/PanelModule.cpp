#include "modules/PanelModule.h"

#include "imgui.h"

namespace editor
{
PanelModule::PanelModule(const char* title) : m_title(title) {}

void PanelModule::onAttach(EditorContext&) {}

void PanelModule::onDetach(EditorContext&) {}

void PanelModule::onUpdate(EditorContext&, float) {}

void PanelModule::onImGuiRender(EditorContext& context)
{
    ImGui::Begin(m_title);
    drawContent(context);
    ImGui::End();
}
}  // namespace editor
