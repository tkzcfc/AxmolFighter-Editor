#include "editor_properties/Vec2EditorProperty.h"

#include "imgui.h"

namespace editor
{
Vec2EditorProperty::Vec2EditorProperty(std::string id, std::string label, SceneVec2& value)
    : EditorProperty(std::move(id), std::move(label)), m_value(value)
{}

void Vec2EditorProperty::draw(EditorPropertyContext& context)
{
    float data[2] = {m_value.x, m_value.y};
    const bool prepared = beginEdit(context);
    const bool changed = ImGui::DragFloat2(label().c_str(), data);
    if (changed)
    {
        m_value.x = data[0];
        m_value.y = data[1];
    }
    finishEdit(context, prepared, changed);
}
}  // namespace editor
