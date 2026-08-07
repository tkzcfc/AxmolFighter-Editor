#include "editor_properties/FloatEditorProperty.h"

#include "imgui.h"

namespace editor
{
FloatEditorProperty::FloatEditorProperty(std::string id, std::string label, float& value, Callback afterEdit)
    : EditorProperty(std::move(id), std::move(label)), m_value(value), m_afterEdit(std::move(afterEdit))
{}

void FloatEditorProperty::draw(EditorPropertyContext& context)
{
    const bool prepared = beginEdit(context);
    const bool changed = ImGui::DragFloat(label().c_str(), &m_value);
    finishEdit(context, prepared, changed);
    if (ImGui::IsItemDeactivatedAfterEdit() && m_afterEdit)
        m_afterEdit(context);
}
}  // namespace editor
