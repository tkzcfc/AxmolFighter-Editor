#include "editor_properties/MultilineStringEditorProperty.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <cfloat>

namespace editor
{
MultilineStringEditorProperty::MultilineStringEditorProperty(std::string id,
                                                           std::string label,
                                                           std::string& value,
                                                           Callback onChanged,
                                                           Callback afterEdit)
    : EditorProperty(std::move(id), std::move(label))
    , m_value(value)
    , m_onChanged(std::move(onChanged))
    , m_afterEdit(std::move(afterEdit))
{}

void MultilineStringEditorProperty::draw(EditorPropertyContext& context)
{
    ImGui::TextUnformatted(label().c_str());
    const bool prepared = beginEdit(context);
    const bool changed =
        ImGui::InputTextMultiline(("##" + id()).c_str(), &m_value, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f));
    if (changed && m_onChanged)
        m_onChanged(context);
    finishEdit(context, prepared, changed);
    if (ImGui::IsItemDeactivatedAfterEdit() && m_afterEdit)
        m_afterEdit(context);
}
}  // namespace editor
