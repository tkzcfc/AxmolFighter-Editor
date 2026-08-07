#include "editor_properties/OptionalStringEnumEditorProperty.h"

#include "imgui.h"

#include <algorithm>

namespace editor
{
OptionalStringEnumEditorProperty::OptionalStringEnumEditorProperty(std::string id,
                                                                 std::string label,
                                                                 std::string& value,
                                                                 std::vector<std::string> items,
                                                                 Callback onChanged)
    : EditorProperty(std::move(id), std::move(label))
    , m_value(value)
    , m_items(std::move(items))
    , m_onChanged(std::move(onChanged))
{}

void OptionalStringEnumEditorProperty::draw(EditorPropertyContext& context)
{
    if (!m_value.empty() && std::find(m_items.begin(), m_items.end(), m_value) == m_items.end())
    {
        context.document.beginUndoTransaction();
        m_value.clear();
        context.document.markDirty();
        context.document.commitUndoTransaction();
    }

    const std::string preview = m_value.empty() ? "None" : m_value;
    if (!ImGui::BeginCombo(label().c_str(), preview.c_str()))
        return;

    if (ImGui::Selectable("None", m_value.empty()))
    {
        context.document.beginUndoTransaction();
        m_value.clear();
        if (m_onChanged)
            m_onChanged(context);
        context.document.markDirty();
        context.document.commitUndoTransaction();
    }

    for (const std::string& item : m_items)
    {
        const bool isSelected = m_value == item;
        if (ImGui::Selectable(item.c_str(), isSelected))
        {
            context.document.beginUndoTransaction();
            m_value = item;
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
