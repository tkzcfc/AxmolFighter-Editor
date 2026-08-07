#include "editor_properties/ReadOnlyEditorProperty.h"

#include "imgui.h"

namespace editor
{
ReadOnlyEditorProperty::ReadOnlyEditorProperty(std::string id, std::string label, std::string value)
    : EditorProperty(std::move(id), std::move(label)), m_value(std::move(value))
{}

void ReadOnlyEditorProperty::draw(EditorPropertyContext&)
{
    ImGui::TextDisabled("%s: %s", label().c_str(), m_value.c_str());
}
}  // namespace editor
