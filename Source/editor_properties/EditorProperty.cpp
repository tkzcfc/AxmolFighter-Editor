#include "editor_properties/EditorProperty.h"

#include "imgui.h"

namespace editor
{
EditorProperty::EditorProperty(std::string id, std::string label, bool disableWhenNodeLocked)
    : m_id(std::move(id)), m_label(std::move(label)), m_disableWhenNodeLocked(disableWhenNodeLocked)
{}

const std::string& EditorProperty::id() const
{
    return m_id;
}

const std::string& EditorProperty::label() const
{
    return m_label;
}

void EditorProperty::drawWithState(EditorPropertyContext& context)
{
    const bool disabled = m_disableWhenNodeLocked && context.node.locked;
    if (disabled)
        ImGui::BeginDisabled();
    draw(context);
    if (disabled)
        ImGui::EndDisabled();
}

bool EditorProperty::beginEdit(EditorPropertyContext& context) const
{
    if (!context.activeEditKey.empty() && !context.document.hasActiveUndoTransaction())
        context.activeEditKey.clear();
    if (context.activeEditKey.empty())
        return context.document.beginUndoTransaction();
    return false;
}

void EditorProperty::finishEdit(EditorPropertyContext& context, bool prepared, bool changed) const
{
    if (changed)
        context.document.markDirty();

    if ((ImGui::IsItemActivated() || changed) && context.activeEditKey.empty())
        context.activeEditKey = id();

    const bool ownsEdit = context.activeEditKey == id();
    if (ownsEdit && (ImGui::IsItemDeactivated() || (changed && !ImGui::IsItemActive())))
    {
        context.document.commitUndoTransaction();
        context.activeEditKey.clear();
        return;
    }

    if (prepared && context.activeEditKey.empty())
        context.document.cancelUndoTransaction();
}

void drawEditorPropertyGroups(EditorPropertyContext& context, const std::vector<EditorPropertyGroup>& groups)
{
    bool firstGroup = true;
    for (const EditorPropertyGroup& group : groups)
    {
        if (!firstGroup)
            ImGui::Separator();
        firstGroup = false;

        ImGui::TextUnformatted(group.label.c_str());
        ImGui::Separator();
        for (const std::unique_ptr<EditorProperty>& property : group.properties)
            property->drawWithState(context);
    }
}
}  // namespace editor
