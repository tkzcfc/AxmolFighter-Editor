#include "editor_properties/BoolEditorProperty.h"

#include "imgui.h"

namespace editor
{
BoolEditorProperty::BoolEditorProperty(std::string id,
                                     std::string label,
                                     bool& value,
                                     bool disableWhenNodeLocked,
                                     Callback onChanged)
    : EditorProperty(std::move(id), std::move(label), disableWhenNodeLocked)
    , m_value(value)
    , m_onChanged(std::move(onChanged))
{}

void BoolEditorProperty::draw(EditorPropertyContext& context)
{
    const bool transaction = context.document.beginUndoTransaction();
    const bool changed = ImGui::Checkbox(label().c_str(), &m_value);
    if (changed)
    {
        if (m_onChanged)
            m_onChanged(context);
        context.document.markDirty();
    }
    if (transaction)
        context.document.commitUndoTransaction();
}
}  // namespace editor
