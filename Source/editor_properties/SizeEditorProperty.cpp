#include "editor_properties/SizeEditorProperty.h"

#include "imgui.h"

#include <algorithm>

namespace editor
{
SizeEditorProperty::SizeEditorProperty(std::string id,
                                       std::string label,
                                       SceneSize& value,
                                       SceneNodeSizeEditPolicy policy)
    : EditorProperty(std::move(id), std::move(label)), m_value(value), m_policy(std::move(policy))
{}

void SizeEditorProperty::draw(EditorPropertyContext& context)
{
    ImGui::TextUnformatted(label().c_str());
    ImGui::PushID(id().c_str());

    auto drawAxis = [&](const char* label, const std::string& key, float& value, bool editable) {
        if (!editable)
            ImGui::BeginDisabled();

        if (editable)
        {
            const bool prepared = beginEdit(context);
            const bool changed = ImGui::DragFloat(label, &value);
            if (changed)
                value = std::max(0.0f, value);
            finishEdit(context, prepared, changed);
        }
        else
        {
            float displayValue = value;
            ImGui::InputFloat(label, &displayValue, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
        }

        if (!editable)
        {
            ImGui::EndDisabled();
            if (!m_policy.reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", m_policy.reason.c_str());
        }
    };

    drawAxis("Width", id() + ".width", m_value.width, m_policy.widthEditable);
    drawAxis("Height", id() + ".height", m_value.height, m_policy.heightEditable);
    ImGui::PopID();
}
}  // namespace editor
