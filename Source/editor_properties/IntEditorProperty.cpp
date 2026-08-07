#include "editor_properties/IntEditorProperty.h"

#include "imgui.h"

#include <algorithm>

namespace editor
{
IntEditorProperty::IntEditorProperty(std::string id, std::string label, int& value, int minValue, int maxValue)
    : EditorProperty(std::move(id), std::move(label)), m_value(value), m_minValue(minValue), m_maxValue(maxValue)
{}

void IntEditorProperty::draw(EditorPropertyContext& context)
{
    const bool prepared = beginEdit(context);
    const bool changed = ImGui::DragInt(label().c_str(), &m_value, 1.0f, m_minValue, m_maxValue);
    if (changed)
        m_value = std::clamp(m_value, m_minValue, m_maxValue);
    finishEdit(context, prepared, changed);
}
}  // namespace editor
