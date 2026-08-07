#include "editor_properties/AssetReferenceEditorProperty.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>

namespace editor
{
AssetReferenceEditorProperty::AssetReferenceEditorProperty(std::string id,
                                                         std::string label,
                                                         std::string& value,
                                                         std::initializer_list<AssetKind> allowedKinds,
                                                         AssignCallback assign,
                                                         AfterEditCallback afterEdit)
    : EditorProperty(std::move(id), std::move(label))
    , m_value(value)
    , m_allowedKinds(allowedKinds)
    , m_assign(std::move(assign))
    , m_afterEdit(std::move(afterEdit))
{}

void AssetReferenceEditorProperty::draw(EditorPropertyContext& context)
{
    const bool prepared = beginEdit(context);
    const bool changed = ImGui::InputText(label().c_str(), &m_value);
    finishEdit(context, prepared, changed);
    if (ImGui::IsItemDeactivatedAfterEdit() && m_afterEdit)
        m_afterEdit(context);

    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayloadType))
    {
        if (payload->DataSize == sizeof(AssetDragPayload))
        {
            const auto* asset = static_cast<const AssetDragPayload*>(payload->Data);
            if (std::find(m_allowedKinds.begin(), m_allowedKinds.end(), asset->kind) != m_allowedKinds.end())
            {
                context.document.beginUndoTransaction();
                if (m_assign)
                    m_assign(context, *asset);
                context.document.markDirty();
                context.document.commitUndoTransaction();
                if (m_afterEdit)
                    m_afterEdit(context);
            }
        }
    }
    ImGui::EndDragDropTarget();
}
}  // namespace editor
