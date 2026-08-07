#include "editor_properties/EnumEditorProperty.h"

#include "imgui.h"

namespace editor
{
EnumEditorProperty::EnumEditorProperty(std::string id,
                                     std::string label,
                                     std::string& value,
                                     std::vector<std::string> items,
                                     Callback onChanged)
    : EditorProperty(std::move(id), std::move(label))
    , m_value(value)
    , m_items(std::move(items))
    , m_onChanged(std::move(onChanged))
{}

EnumEditorProperty::EnumEditorProperty(std::string id,
                                       std::string label,
                                       std::string& value,
                                       std::vector<std::string> items,
                                       std::string emptyDisplayValue,
                                       Callback onChanged)
    : EditorProperty(std::move(id), std::move(label))
    , m_value(value)
    , m_items(std::move(items))
    , m_emptyDisplayValue(std::move(emptyDisplayValue))
    , m_onChanged(std::move(onChanged))
{}

void EnumEditorProperty::draw(EditorPropertyContext& context)
{
    int selected = 0;
    const std::string& effectiveValue = m_value.empty() && !m_emptyDisplayValue.empty() ? m_emptyDisplayValue : m_value;
    for (int index = 0; index < static_cast<int>(m_items.size()); ++index)
    {
        if (effectiveValue == m_items[index])
        {
            selected = index;
            break;
        }
    }

    const char* preview = m_items.empty() ? "" : m_items[selected].c_str();
    if (!ImGui::BeginCombo(label().c_str(), preview))
        return;

    for (int index = 0; index < static_cast<int>(m_items.size()); ++index)
    {
        const bool isSelected = selected == index;
        if (ImGui::Selectable(m_items[index].c_str(), isSelected))
        {
            context.document.beginUndoTransaction();
            m_value = m_items[index];
            if (m_onChanged)
                m_onChanged(context);
            context.document.markDirty();
            context.document.commitUndoTransaction();
        }
        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}
}  // namespace editor
