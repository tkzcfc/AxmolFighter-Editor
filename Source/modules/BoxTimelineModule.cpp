#include "modules/BoxTimelineModule.h"

#include "combat/BoxEditorSessionService.h"
#include "core/EditorContext.h"
#include "documents/BoxDocument.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace editor
{
namespace
{
CombatBox defaultCombatBox()
{
    return {{0, 0, 0}, {40, 10, 80}};
}

std::size_t upsertKey(BoxTrack& track, BoxKey key)
{
    const auto it = std::lower_bound(track.keys.begin(), track.keys.end(), key.timeMs,
                                     [](const BoxKey& item, std::int32_t timeMs) {
                                         return item.timeMs < timeMs;
                                     });
    if (it != track.keys.end() && it->timeMs == key.timeMs)
    {
        *it = std::move(key);
        return static_cast<std::size_t>(it - track.keys.begin());
    }
    return static_cast<std::size_t>(track.keys.insert(it, std::move(key)) - track.keys.begin());
}

const BoxKey* keyBefore(const BoxTrack& track, std::int32_t timeMs)
{
    const auto it = std::lower_bound(track.keys.begin(), track.keys.end(), timeMs,
                                     [](const BoxKey& item, std::int32_t value) {
                                         return item.timeMs < value;
                                     });
    return it == track.keys.begin() ? nullptr : &*(it - 1);
}

void drawDiamond(ImDrawList& drawList, const ImVec2& center, float radius, ImU32 color, bool filled)
{
    const std::array<ImVec2, 4> points = {
        ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y),
        ImVec2(center.x, center.y + radius), ImVec2(center.x - radius, center.y)};
    if (filled)
        drawList.AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), color);
    drawList.AddPolyline(points.data(), static_cast<int>(points.size()), color, ImDrawFlags_Closed, 1.5f);
}

bool isKeyHit(const ImVec2& mouse, float x, float y)
{
    return std::abs(mouse.x - x) <= 9.0f && std::abs(mouse.y - y) <= 11.0f;
}
}  // namespace

std::int32_t BoxTimelineEditor::snapTime(EditorContext& context, std::int32_t value) const
{
    auto session = context.services().get<BoxEditorSessionService>();
    auto* document = session ? session->document() : nullptr;
    if (!session || !document)
        return value;

    const std::int32_t duration = document->data().duration;
    if (duration <= 0)
        return 0;
    if (!session->preview().isAni() || !session->preview().isLoaded() || session->preview().durationMs() <= 0)
        return std::clamp(value, 0, duration - 1);

    std::int32_t best = std::clamp(value, 0, duration - 1);
    std::int32_t bestDistance = std::abs(best - value);
    for (std::int32_t boundary : session->preview().frameBoundaries())
    {
        if (boundary >= duration)
            continue;
        const std::int32_t distance = std::abs(boundary - value);
        if (distance < bestDistance)
        {
            best = boundary;
            bestDistance = distance;
        }
    }
    return std::clamp(best, 0, duration - 1);
}

void BoxTimelineEditor::finishDrag()
{
    m_dragMode = DragMode::None;
    m_dragTrack = -1;
    m_dragKey = -1;
}

void BoxTimelineEditor::draw(EditorContext& context, BoxDocument& boxDocument)
{
    auto session = context.services().get<BoxEditorSessionService>();
    if (!session)
    {
        ImGui::TextDisabled("Box editor session is not available.");
        return;
    }
    BoxDocument& document = boxDocument;
    const std::int32_t documentDuration = document.data().duration;

    if (ImGui::Button(session->isPlaying() ? "Pause" : "Play"))
        session->setPlaying(!session->isPlaying());
    ImGui::SameLine();
    ImGui::Text("%d / %d ms", session->playheadMs(), documentDuration);
    ImGui::SameLine();
    const bool canAddKey = session->selectedTrack >= 0 &&
                           session->selectedTrack < static_cast<int>(document.data().tracks.size()) &&
                           documentDuration > 0;
    ImGui::BeginDisabled(!canAddKey);
    if (ImGui::Button("Add Key at Playhead"))
    {
        BoxTrack& track = document.data().tracks[session->selectedTrack];
        const std::int32_t timeMs = session->playheadMs();
        const BoxKey* previous = keyBefore(track, timeMs);
        BoxKey key{timeMs, previous && previous->box ? previous->box : std::optional<CombatBox>(defaultCombatBox())};
        session->selectedKey = static_cast<int>(upsertKey(track, std::move(key)));
        document.markDirty();
    }
    ImGui::EndDisabled();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_K) && canAddKey)
    {
        BoxTrack& track = document.data().tracks[session->selectedTrack];
        const std::int32_t timeMs = session->playheadMs();
        const BoxKey* previous = keyBefore(track, timeMs);
        BoxKey key{timeMs, previous && previous->box ? previous->box : std::optional<CombatBox>(defaultCombatBox())};
        session->selectedKey = static_cast<int>(upsertKey(track, std::move(key)));
        document.markDirty();
    }
    ImGui::Separator();

    const float labelWidth = 130.0f;
    const float timelineWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x - labelWidth);
    const float rulerHeight = 26.0f;
    const float duration = static_cast<float>(std::max(1, documentDuration));
    if (ImGui::BeginChild("BoxTimelineRegion", ImVec2(0.0f, 0.0f), false,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImGui::BeginTable("BoxTimelineTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("TrackName", ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("TrackTimeline", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("Time / Track");
            ImGui::TableSetColumnIndex(1);
            const ImVec2 rulerMin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("BoxTimelineRuler", ImVec2(timelineWidth, rulerHeight),
                                   ImGuiButtonFlags_MouseButtonLeft);
            const ImVec2 rulerMax = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(rulerMin, rulerMax, IM_COL32(35, 39, 46, 255));
            for (int tick = 0; tick <= 10; ++tick)
            {
                const float x = rulerMin.x + timelineWidth * static_cast<float>(tick) / 10.0f;
                drawList->AddLine(ImVec2(x, rulerMin.y + 15.0f), ImVec2(x, rulerMax.y), IM_COL32(140, 145, 155, 180));
                const int time = static_cast<int>(std::lround(static_cast<double>(documentDuration) * tick / 10.0));
                drawList->AddText(ImVec2(x + 2.0f, rulerMin.y + 1.0f), IM_COL32(190, 195, 205, 230),
                                  std::to_string(time).c_str());
            }
            if (session->preview().isAni() && session->preview().isLoaded())
            {
                for (std::int32_t boundary : session->preview().frameBoundaries())
                {
                    if (boundary > documentDuration)
                        continue;
                    const float x = rulerMin.x + timelineWidth * static_cast<float>(boundary) / duration;
                    drawList->AddLine(ImVec2(x, rulerMin.y), ImVec2(x, rulerMax.y), IM_COL32(120, 160, 210, 80));
                }
            }
            for (std::size_t eventIndex = 0; eventIndex < document.data().events.size(); ++eventIndex)
            {
                const BoxEvent& event = document.data().events[eventIndex];
                const float x = rulerMin.x + timelineWidth * static_cast<float>(event.timeMs) / duration;
                drawList->AddLine(ImVec2(x, rulerMin.y + 4.0f), ImVec2(x, rulerMax.y),
                                  session->selectedEvent == static_cast<int>(eventIndex)
                                      ? IM_COL32(255, 235, 80, 255)
                                      : IM_COL32(240, 180, 60, 220), 2.0f);
            }
            const float playheadX = rulerMin.x + timelineWidth * static_cast<float>(session->playheadMs()) / duration;
            drawList->AddLine(ImVec2(playheadX, rulerMin.y), ImVec2(playheadX, rulerMax.y), IM_COL32(255, 90, 90, 255), 2.0f);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const int time = static_cast<int>(std::lround(
                    (ImGui::GetIO().MousePos.x - rulerMin.x) / timelineWidth * documentDuration));
                session->setPlayheadMs(std::clamp(time, 0, std::max(0, documentDuration - 1)));
                session->selectedTrack = -1;
                session->selectedKey = -1;
                session->selectedEvent = -1;
            }

            for (std::size_t trackIndex = 0; trackIndex < document.data().tracks.size(); ++trackIndex)
            {
                BoxTrack& track = document.data().tracks[trackIndex];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(static_cast<int>(trackIndex));
                if (ImGui::Selectable(track.name.c_str(), session->selectedTrack == static_cast<int>(trackIndex)))
                {
                    session->selectedTrack = static_cast<int>(trackIndex);
                    session->selectedKey = -1;
                    session->selectedEvent = -1;
                }
                ImGui::TextDisabled("%s", track.kind.c_str());
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                ImGui::PushID(static_cast<int>(trackIndex));
                ImGui::InvisibleButton("BoxKeyTimelineRow", ImVec2(timelineWidth, 30.0f),
                                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                const ImVec2 rowMax = ImGui::GetItemRectMax();
                const bool rowHovered = ImGui::IsItemHovered();
                ImGui::PopID();
                drawList->AddRectFilled(rowMin, rowMax, IM_COL32(31, 34, 40, 255));

                for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex)
                {
                    const BoxKey& key = track.keys[keyIndex];
                    const float x = rowMin.x + timelineWidth * static_cast<float>(key.timeMs) / duration;
                    const float centerY = (rowMin.y + rowMax.y) * 0.5f;
                    if (key.box)
                    {
                        const std::int32_t endTime = keyIndex + 1 < track.keys.size()
                                                         ? track.keys[keyIndex + 1].timeMs
                                                         : documentDuration;
                        const float endX = rowMin.x + timelineWidth * static_cast<float>(endTime) / duration;
                        const ImU32 rangeColor = track.kind == "attack" ? IM_COL32(190, 50, 50, 72)
                                              : track.kind == "hitbox"  ? IM_COL32(200, 150, 30, 72)
                                                                        : IM_COL32(50, 105, 190, 72);
                        drawList->AddRectFilled(ImVec2(x, centerY - 3.0f), ImVec2(endX, centerY + 3.0f), rangeColor);
                    }
                    const bool selected = session->selectedTrack == static_cast<int>(trackIndex) &&
                                          session->selectedKey == static_cast<int>(keyIndex);
                    const ImU32 keyColor = selected ? IM_COL32(255, 235, 100, 255)
                                                     : (key.box ? IM_COL32(230, 235, 245, 255)
                                                                : IM_COL32(165, 175, 190, 255));
                    drawDiamond(*drawList, ImVec2(x, centerY), selected ? 7.0f : 5.5f, keyColor, key.box.has_value());
                }
                const float rowPlayheadX = rowMin.x + timelineWidth * static_cast<float>(session->playheadMs()) / duration;
                drawList->AddLine(ImVec2(rowPlayheadX, rowMin.y), ImVec2(rowPlayheadX, rowMax.y), IM_COL32(255, 90, 90, 180), 1.0f);

                int hitKey = -1;
                if (rowHovered)
                {
                    const float centerY = (rowMin.y + rowMax.y) * 0.5f;
                    for (int keyIndex = static_cast<int>(track.keys.size()) - 1; keyIndex >= 0; --keyIndex)
                    {
                        const float x = rowMin.x + timelineWidth * static_cast<float>(track.keys[keyIndex].timeMs) / duration;
                        if (isKeyHit(ImGui::GetIO().MousePos, x, centerY))
                        {
                            hitKey = keyIndex;
                            break;
                        }
                    }
                }
                if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_dragMode == DragMode::None)
                {
                    session->selectedTrack = static_cast<int>(trackIndex);
                    session->selectedEvent = -1;
                    if (hitKey >= 0)
                    {
                        session->selectedKey = hitKey;
                        session->setPlayheadMs(track.keys[hitKey].timeMs);
                        m_dragMode = DragMode::Key;
                        m_dragTrack = static_cast<int>(trackIndex);
                        m_dragKey = hitKey;
                        m_dragOriginalKey = track.keys[hitKey];
                        m_dragStartMouseTime = static_cast<std::int32_t>(std::lround(
                            (ImGui::GetIO().MousePos.x - rowMin.x) / timelineWidth * documentDuration));
                        document.beginEditTransaction();
                    }
                    else
                    {
                        session->selectedKey = -1;
                        const int time = static_cast<int>(std::lround(
                            (ImGui::GetIO().MousePos.x - rowMin.x) / timelineWidth * documentDuration));
                        session->setPlayheadMs(std::clamp(time, 0, std::max(0, documentDuration - 1)));
                    }
                }
                if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hitKey >= 0)
                {
                    m_contextTrack = static_cast<int>(trackIndex);
                    m_contextKey = hitKey;
                    ImGui::PushID(static_cast<int>(trackIndex));
                    ImGui::OpenPopup("BoxKeyContext");
                    ImGui::PopID();
                }
                ImGui::PushID(static_cast<int>(trackIndex));
                if (ImGui::BeginPopup("BoxKeyContext"))
                {
                    if (m_contextTrack == static_cast<int>(trackIndex) && m_contextKey >= 0 &&
                        m_contextKey < static_cast<int>(track.keys.size()))
                    {
                        BoxKey& key = track.keys[m_contextKey];
                        if (ImGui::MenuItem("Copy"))
                            session->setKeyClipboard(key);
                        if (ImGui::MenuItem("Paste at Playhead", nullptr, false,
                                            session->hasKeyClipboard() && documentDuration > 0))
                        {
                            BoxKey pasted = session->keyClipboard();
                            pasted.timeMs = session->playheadMs();
                            session->selectedKey = static_cast<int>(upsertKey(track, std::move(pasted)));
                            session->selectedTrack = static_cast<int>(trackIndex);
                            document.markDirty();
                        }
                        if (ImGui::MenuItem("Set Null"))
                        {
                            key.box.reset();
                            document.markDirty();
                        }
                        if (ImGui::MenuItem("Delete"))
                        {
                            track.keys.erase(track.keys.begin() + m_contextKey);
                            if (session->selectedTrack == static_cast<int>(trackIndex) &&
                                session->selectedKey == m_contextKey)
                                session->selectedKey = -1;
                            document.markDirty();
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();

                if (m_dragMode == DragMode::Key && m_dragTrack == static_cast<int>(trackIndex) &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_dragKey >= 0 &&
                    m_dragKey < static_cast<int>(track.keys.size()))
                {
                    const int currentMouseTime = static_cast<int>(std::lround(
                        (ImGui::GetIO().MousePos.x - rowMin.x) / timelineWidth * documentDuration));
                    BoxKey moved = m_dragOriginalKey;
                    moved.timeMs = snapTime(context, m_dragOriginalKey.timeMs + currentMouseTime - m_dragStartMouseTime);
                    track.keys.erase(track.keys.begin() + m_dragKey);
                    m_dragKey = static_cast<int>(upsertKey(track, std::move(moved)));
                    session->selectedTrack = static_cast<int>(trackIndex);
                    session->selectedKey = m_dragKey;
                    session->setPlayheadMs(track.keys[m_dragKey].timeMs);
                    document.markDirty();
                }
            }
            if (m_dragMode != DragMode::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                document.commitEditTransaction();
                finishDrag();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}
}  // namespace editor
