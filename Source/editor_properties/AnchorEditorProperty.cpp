#include "editor_properties/AnchorEditorProperty.h"

#include "imgui.h"

#include <algorithm>
#include <utility>

namespace editor
{
AnchorEditorProperty::AnchorEditorProperty(std::string id, std::string label, SceneVec2& value)
    : EditorProperty(std::move(id), std::move(label)), m_value(value)
{}

void AnchorEditorProperty::draw(EditorPropertyContext& context)
{
    ImGui::PushID(id().c_str());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label().c_str());
    ImGui::SameLine();

    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonWidth = ImGui::GetFrameHeight();
    const float inputWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x - buttonWidth - style.ItemInnerSpacing.x);

    float data[2] = {m_value.x, m_value.y};
    const bool prepared = beginEdit(context);
    ImGui::SetNextItemWidth(inputWidth);
    const bool changed = ImGui::InputFloat2("##value", data);
    if (changed)
    {
        m_value.x = data[0];
        m_value.y = data[1];
    }
    finishEdit(context, prepared, changed);

    ImGui::SameLine();
    if (ImGui::ArrowButton("##presets", ImGuiDir_Down))
        ImGui::OpenPopup("AnchorPresets");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Anchor presets");

    if (ImGui::BeginPopup("AnchorPresets"))
    {
        struct Preset
        {
            const char* label;
            SceneVec2 value;
        };

        constexpr Preset presets[] = {
            {"Center", {0.5f, 0.5f}},
            {"Top Left", {0.0f, 1.0f}},
            {"Top Right", {1.0f, 1.0f}},
            {"Bottom Left", {0.0f, 0.0f}},
            {"Bottom Right", {1.0f, 0.0f}},
        };

        for (const Preset& preset : presets)
        {
            if (ImGui::MenuItem(preset.label))
                setAnchor(context, preset.value);
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

bool AnchorEditorProperty::setAnchor(EditorPropertyContext& context, const SceneVec2& value)
{
    if (m_value.x == value.x && m_value.y == value.y)
        return false;

    const bool began = context.document.beginUndoTransaction();
    m_value = value;
    context.document.markDirty();
    if (began || context.document.hasActiveUndoTransaction())
        context.document.commitUndoTransaction();
    context.activeEditKey.clear();
    return true;
}
}  // namespace editor
