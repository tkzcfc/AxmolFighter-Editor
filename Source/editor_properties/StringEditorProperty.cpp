#include "editor_properties/StringEditorProperty.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

namespace editor
{
StringEditorProperty::StringEditorProperty(std::string id, std::string label, std::string& value, Callback afterEdit)
    : EditorProperty(std::move(id), std::move(label)), m_value(value), m_afterEdit(std::move(afterEdit))
{}

void StringEditorProperty::draw(EditorPropertyContext& context)
{
    const bool prepared = beginEdit(context);
    const bool changed = ImGui::InputText(label().c_str(), &m_value);
    finishEdit(context, prepared, changed);
    if (ImGui::IsItemDeactivatedAfterEdit() && m_afterEdit)
        m_afterEdit(context);
}
}  // namespace editor
