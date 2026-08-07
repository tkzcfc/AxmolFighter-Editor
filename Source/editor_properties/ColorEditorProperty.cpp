#include "editor_properties/ColorEditorProperty.h"

#include "imgui.h"

namespace editor
{
ColorEditorProperty::ColorEditorProperty(std::string id, std::string label, SceneColor& value)
    : EditorProperty(std::move(id), std::move(label)), m_value(value)
{}

void ColorEditorProperty::draw(EditorPropertyContext& context)
{
    float data[4] = {m_value.r, m_value.g, m_value.b, m_value.a};
    const bool prepared = beginEdit(context);
    const bool changed = ImGui::ColorEdit4(label().c_str(), data);
    if (changed)
    {
        m_value.r = data[0];
        m_value.g = data[1];
        m_value.b = data[2];
        m_value.a = data[3];
    }
    finishEdit(context, prepared, changed);
}
}  // namespace editor
