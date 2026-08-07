#include "motion/MotionEditor.h"

#include "asset_browser/AssetDragPayload.h"
#include "asset_browser/AssetKind.h"
#include "core/EditorContext.h"
#include "documents/MotionDocument.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace editor
{
namespace
{
constexpr const char* kMotionAnimationReorderPayload = "MOTION_ANIMATION_INDEX";

bool containsIgnoreCase(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return true;
    auto lower = [](unsigned char character) { return static_cast<char>(std::tolower(character)); };
    std::string left = haystack;
    std::string right = needle;
    std::transform(left.begin(), left.end(), left.begin(), lower);
    std::transform(right.begin(), right.end(), right.begin(), lower);
    return left.find(right) != std::string::npos;
}

std::string uniqueMotionName(const MotionData& data, const std::string& preferred)
{
    std::string candidate = preferred.empty() ? "motion" : preferred;
    if (std::none_of(data.motions.begin(), data.motions.end(),
                     [&](const MotionEntry& entry) { return entry.name == candidate; }))
        return candidate;

    for (int index = 2;; ++index)
    {
        const std::string numbered = preferred.empty() ? ("motion_" + std::to_string(index))
                                                       : (preferred + "_" + std::to_string(index));
        if (std::none_of(data.motions.begin(), data.motions.end(),
                         [&](const MotionEntry& entry) { return entry.name == numbered; }))
            return numbered;
    }
}

std::string uniqueAnimationId(const MotionEntry& motion, const std::string& preferred)
{
    std::string candidate = preferred.empty() ? "entry" : preferred;
    if (std::none_of(motion.animations.begin(), motion.animations.end(),
                     [&](const MotionAnimationEntry& entry) { return entry.id == candidate; }))
        return candidate;

    for (int index = 2;; ++index)
    {
        const std::string numbered = preferred.empty() ? ("entry_" + std::to_string(index))
                                                       : (preferred + "_" + std::to_string(index));
        if (std::none_of(motion.animations.begin(), motion.animations.end(),
                         [&](const MotionAnimationEntry& entry) { return entry.id == numbered; }))
            return numbered;
    }
}

MotionEntry* findMotionByName(MotionData& data, const std::string& name)
{
    const auto it = std::find_if(data.motions.begin(), data.motions.end(),
                                 [&](const MotionEntry& entry) { return entry.name == name; });
    return it == data.motions.end() ? nullptr : &*it;
}
}  // namespace

void MotionEditor::drawContent(EditorContext& context, MotionDocument& document)
{
    syncSelection(document);

    const float listWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.28f, 180.0f, 320.0f);
    if (ImGui::BeginChild("MotionListColumn", ImVec2(listWidth, 0.0f), true))
        drawMotionList(document);
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("MotionAnimationColumn", ImVec2(0.0f, 0.0f), true))
        drawAnimationTable(context, document);
    ImGui::EndChild();
}

void MotionEditor::drawInspector(EditorContext& context, MotionDocument& document)
{
    syncSelection(document);

    MotionEntry* motion = findMotionByName(document.data(), m_selectedMotionName);
    if (!motion)
    {
        ImGui::TextUnformatted("Select a motion entry.");
        return;
    }
    if (m_selectedAnimation < 0 || m_selectedAnimation >= static_cast<int>(motion->animations.size()))
    {
        ImGui::TextUnformatted("Select an animation entry.");
        return;
    }

    MotionAnimationEntry& entry = motion->animations[static_cast<std::size_t>(m_selectedAnimation)];
    ImGui::SeparatorText("Selected Animation");

    if (ImGui::InputText("Id", &entry.id))
        document.markDirty();

    const char* typeItems[] = {"ani", "spine"};
    int typeIndex           = entry.type == MotionAnimationType::Spine ? 1 : 0;
    if (ImGui::Combo("Type", &typeIndex, typeItems, 2))
    {
        entry.type = typeIndex == 1 ? MotionAnimationType::Spine : MotionAnimationType::Ani;
        document.markDirty();
    }

    if (ImGui::InputText("Source", &entry.source))
        document.markDirty();
    if (entry.type == MotionAnimationType::Ani)
        acceptAniDrop(document, entry);

    if (ImGui::InputText("Box", &entry.box))
        document.markDirty();
    acceptBoxDrop(document, entry.box);
    if (ImGui::InputText("Tag", &entry.tag))
        document.markDirty();

    ImGui::Spacing();
    ImGui::TextDisabled("Type selects ani file or spine animation name in Source. Drag .ani / .box onto Source / Box.");
    (void)context;
}

void MotionEditor::reset()
{
    m_documentPath.clear();
    m_motionFilter.clear();
    m_selectedMotionName.clear();
    m_selectedAnimation = -1;
    m_boundRevision = 0;
}

void MotionEditor::drawMotionList(MotionDocument& document)
{
    ImGui::SeparatorText("Motions");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##MotionFilter", "Search...", &m_motionFilter);

    if (ImGui::Button("Add Motion"))
    {
        MotionEntry motion;
        motion.name = uniqueMotionName(document.data(), "motion");
        document.data().motions.push_back(std::move(motion));
        m_selectedMotionName = document.data().motions.back().name;
        m_selectedAnimation = -1;
        document.markDirty();
    }
    ImGui::SameLine();
    const bool canRemove = findMotionByName(document.data(), m_selectedMotionName) != nullptr;
    if (!canRemove)
        ImGui::BeginDisabled();
    if (ImGui::Button("Delete") && canRemove)
    {
        auto& motions = document.data().motions;
        motions.erase(std::remove_if(motions.begin(), motions.end(),
                                     [&](const MotionEntry& entry) { return entry.name == m_selectedMotionName; }),
                      motions.end());
        m_selectedMotionName.clear();
        m_selectedAnimation = -1;
        document.markDirty();
    }
    if (!canRemove)
        ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::BeginChild("MotionSelectableList", ImVec2(0.0f, 0.0f), false))
    {
        for (std::size_t index = 0; index < document.data().motions.size(); ++index)
        {
            MotionEntry& motion = document.data().motions[index];
            if (!containsIgnoreCase(motion.name, m_motionFilter))
                continue;

            ImGui::PushID(static_cast<int>(index));
            const std::string label = motion.name + " (" + std::to_string(motion.animations.size()) + ")";
            const bool selected = motion.name == m_selectedMotionName;
            if (ImGui::Selectable(label.c_str(), selected))
            {
                m_selectedMotionName = motion.name;
                m_selectedAnimation = motion.animations.empty() ? -1 : 0;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void MotionEditor::drawAnimationTable(EditorContext& context, MotionDocument& document)
{
    MotionEntry* motion = findMotionByName(document.data(), m_selectedMotionName);
    if (!motion)
    {
        ImGui::TextUnformatted("Select or create a motion on the left.");
        return;
    }

    ImGui::SeparatorText(motion->name.c_str());
    if (ImGui::InputText("Motion Name", &motion->name))
    {
        m_selectedMotionName = motion->name;
        document.markDirty();
    }

    if (ImGui::Button("Add Entry"))
    {
        MotionAnimationEntry entry;
        entry.id     = uniqueAnimationId(*motion, "entry");
        entry.type   = MotionAnimationType::Ani;
        entry.source = "Untitled.ani";
        motion->animations.push_back(std::move(entry));
        m_selectedAnimation = static_cast<int>(motion->animations.size()) - 1;
        document.markDirty();
    }
    ImGui::SameLine();
    const bool canRemoveEntry =
        m_selectedAnimation >= 0 && m_selectedAnimation < static_cast<int>(motion->animations.size());
    if (!canRemoveEntry)
        ImGui::BeginDisabled();
    if (ImGui::Button("Delete Entry") && canRemoveEntry)
    {
        motion->animations.erase(motion->animations.begin() + m_selectedAnimation);
        if (motion->animations.empty())
            m_selectedAnimation = -1;
        else
            m_selectedAnimation = std::min(m_selectedAnimation, static_cast<int>(motion->animations.size()) - 1);
        document.markDirty();
    }
    if (!canRemoveEntry)
        ImGui::EndDisabled();

    ImGui::Spacing();
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                           ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("MotionAnimations", 5, tableFlags))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Box");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        bool reordered = false;
        for (std::size_t index = 0; index < motion->animations.size(); ++index)
        {
            MotionAnimationEntry& entry = motion->animations[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const bool selected = m_selectedAnimation == static_cast<int>(index);
            if (ImGui::Selectable(std::to_string(index + 1).c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                m_selectedAnimation = static_cast<int>(index);

            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(kMotionAnimationReorderPayload, &index, sizeof(index));
                ImGui::Text("Move #%zu", index + 1);
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kMotionAnimationReorderPayload);
                    payload && payload->DataSize == sizeof(std::size_t))
                {
                    const std::size_t from = *static_cast<const std::size_t*>(payload->Data);
                    if (from < motion->animations.size() && from != index)
                    {
                        MotionAnimationEntry moved = std::move(motion->animations[from]);
                        motion->animations.erase(motion->animations.begin() + static_cast<std::ptrdiff_t>(from));
                        const std::size_t target = from < index ? index - 1 : index;
                        motion->animations.insert(motion->animations.begin() + static_cast<std::ptrdiff_t>(target),
                                                  std::move(moved));
                        m_selectedAnimation = static_cast<int>(target);
                        document.markDirty();
                        reordered = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.id.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.type == MotionAnimationType::Spine ? "spine" : "ani");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(entry.source.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(entry.box.c_str());
            ImGui::PopID();
            if (reordered)
                break;
        }
        ImGui::EndTable();
    }
    (void)context;
}

void MotionEditor::syncSelection(MotionDocument& document)
{
    const std::string path = document.path().generic_string();
    if (path != m_documentPath)
    {
        m_documentPath = path;
        m_selectedMotionName.clear();
        m_selectedAnimation = -1;
        m_boundRevision = document.revision();
    }

    if (m_boundRevision != document.revision())
        m_boundRevision = document.revision();

    if (!m_selectedMotionName.empty() && findMotionByName(document.data(), m_selectedMotionName) == nullptr)
    {
        m_selectedMotionName.clear();
        m_selectedAnimation = -1;
    }

    if (m_selectedMotionName.empty() && !document.data().motions.empty())
    {
        m_selectedMotionName = document.data().motions.front().name;
        m_selectedAnimation = document.data().motions.front().animations.empty() ? -1 : 0;
    }

    if (MotionEntry* motion = findMotionByName(document.data(), m_selectedMotionName))
    {
        if (motion->animations.empty())
            m_selectedAnimation = -1;
        else if (m_selectedAnimation < 0 || m_selectedAnimation >= static_cast<int>(motion->animations.size()))
            m_selectedAnimation = 0;
    }
}

bool MotionEditor::acceptAniDrop(MotionDocument& document, MotionAnimationEntry& entry)
{
    bool changed = false;
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayloadType);
            payload && payload->DataSize == sizeof(AssetDragPayload))
        {
            const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
            if (asset.kind == AssetKind::FrameAnimation)
            {
                document.beginEditTransaction();
                entry.type   = MotionAnimationType::Ani;
                entry.source = std::filesystem::path(asset.relativePath).filename().generic_string();
                document.markDirty();
                document.commitEditTransaction();
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}

bool MotionEditor::acceptBoxDrop(MotionDocument& document, std::string& boxField)
{
    bool changed = false;
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayloadType);
            payload && payload->DataSize == sizeof(AssetDragPayload))
        {
            const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
            if (asset.kind == AssetKind::Box)
            {
                document.beginEditTransaction();
                boxField = asset.relativePath;
                document.markDirty();
                document.commitEditTransaction();
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}
}  // namespace editor
