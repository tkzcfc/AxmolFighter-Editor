#include "editor_properties/ButtonEditorProperty.h"

#include "imgui.h"

namespace editor
{
ButtonEditorProperty::ButtonEditorProperty(std::string id, std::string label, Callback onClicked)
    : EditorProperty(std::move(id), std::move(label)), m_onClicked(std::move(onClicked))
{}

void ButtonEditorProperty::draw(EditorPropertyContext& context)
{
    if (ImGui::Button(label().c_str()) && m_onClicked)
        m_onClicked(context);
}
}  // namespace editor
