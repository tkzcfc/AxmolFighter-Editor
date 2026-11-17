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

int nextNiceStep(int step)
{
    step = std::max(step, 1);
    int magnitude = 1;
    while (magnitude <= step / 10)
        magnitude *= 10;
    const int residue = step / magnitude;
    if (residue < 2)
        return 2 * magnitude;
    if (residue < 5)
        return 5 * magnitude;
    return 10 * magnitude;
}

// 间距不够时逐级放大到 1、2、5、10……，避免刻度或数字挤在一起。
int chooseStep(float pixelsPerUnit, float minPixels)
{
    int step = 1;
    if (pixelsPerUnit <= 0.0f)
        return step;
    while (static_cast<float>(step) * pixelsPerUnit < minPixels && step < 100000000)
        step = nextNiceStep(step);
    return step;
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

    const bool canTransport = documentDuration > 0;
    ImGui::BeginDisabled(!canTransport);
    if (ImGui::Button("<"))
        session->stepFrame(-1);
    ImGui::SetItemTooltip("Previous Frame");
    ImGui::SameLine();
    if (ImGui::Button(session->isPlaying() ? "Pause" : "Play"))
        session->setPlaying(!session->isPlaying());
    ImGui::SameLine();
    if (ImGui::Button(">"))
        session->stepFrame(1);
    ImGui::SetItemTooltip("Next Frame");
    ImGui::EndDisabled();
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
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        m_dragMode == DragMode::None && !ImGui::IsAnyItemActive())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            session->stepFrame(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            session->stepFrame(1);
        if (ImGui::IsKeyPressed(ImGuiKey_K) && canAddKey)
        {
            BoxTrack& track = document.data().tracks[session->selectedTrack];
            const std::int32_t timeMs = session->playheadMs();
            const BoxKey* previous = keyBefore(track, timeMs);
            BoxKey key{timeMs, previous && previous->box ? previous->box : std::optional<CombatBox>(defaultCombatBox())};
            session->selectedKey = static_cast<int>(upsertKey(track, std::move(key)));
            document.markDirty();
        }
    }
    ImGui::Separator();

    const float labelWidth = 130.0f;
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
            const float trackWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const auto timeToX = [trackWidth, duration](float originX, std::int32_t timeMs) {
                return originX + trackWidth * static_cast<float>(timeMs) / duration;
            };
            const ImVec2 rulerMin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("BoxTimelineRuler", ImVec2(trackWidth, rulerHeight),
                                   ImGuiButtonFlags_MouseButtonLeft);
            const ImVec2 rulerMax = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(rulerMin, rulerMax, IM_COL32(35, 39, 46, 255));
            const float pixelsPerMs = trackWidth / duration;
            const int minorStep = chooseStep(pixelsPerMs, 6.0f);
            int majorStep = minorStep;
            while (static_cast<float>(majorStep) * pixelsPerMs < 44.0f && majorStep <= std::max(documentDuration, 1))
                majorStep = nextNiceStep(majorStep);
            const float playheadX = timeToX(rulerMin.x, session->playheadMs());
            const std::string playheadLabel = std::to_string(session->playheadMs());
            const ImVec2 playheadTextSize = ImGui::CalcTextSize(playheadLabel.c_str());
            const float badgeWidth = playheadTextSize.x + 8.0f;
            const float badgeHeight = playheadTextSize.y + 2.0f;
            float badgeX = playheadX + 5.0f;
            if (badgeX + badgeWidth > rulerMax.x - 2.0f)
                badgeX = std::max(rulerMin.x + 1.0f, playheadX - badgeWidth - 4.0f);
            const ImVec2 badgeMin(badgeX, rulerMin.y + 1.0f);
            const ImVec2 badgeMax(badgeX + badgeWidth, badgeMin.y + badgeHeight);
            const auto labelFits = [&](float textX, const std::string& text) {
                const float textWidth = ImGui::CalcTextSize(text.c_str()).x;
                if (textX < badgeMax.x + 2.0f && textX + textWidth > badgeMin.x - 2.0f)
                    return false;
                return textX + textWidth < rulerMax.x - 1.0f;
            };
            for (std::int32_t time = 0;;)
            {
                const float x = timeToX(rulerMin.x, time);
                const bool atEnd = time == documentDuration;
                const bool major = majorStep > 0 && time % majorStep == 0;
                const float gapFromMajor = static_cast<float>(time % majorStep) * pixelsPerMs;
                if (!major && !atEnd)
                {
                    drawList->AddLine(ImVec2(x, rulerMax.y - 6.0f), ImVec2(x, rulerMax.y), IM_COL32(110, 116, 128, 200));
                }
                else
                {
                    drawList->AddLine(ImVec2(x, rulerMin.y + 16.0f), ImVec2(x, rulerMax.y), IM_COL32(168, 174, 186, 230));
                    const std::string label = std::to_string(time);
                    if ((major || gapFromMajor >= 28.0f) && labelFits(x + 3.0f, label))
                        drawList->AddText(ImVec2(x + 3.0f, rulerMin.y + 1.0f), IM_COL32(190, 195, 205, 230),
                                          label.c_str());
                }
                if (atEnd)
                    break;
                std::int32_t next = time + minorStep;
                if (next > documentDuration)
                    next = documentDuration;
                time = next;
            }
            if (session->preview().isAni() && session->preview().isLoaded())
            {
                for (std::int32_t boundary : session->preview().frameBoundaries())
                {
                    if (boundary > documentDuration)
                        continue;
                    const float x = timeToX(rulerMin.x, boundary);
                    drawList->AddLine(ImVec2(x, rulerMin.y), ImVec2(x, rulerMax.y), IM_COL32(120, 160, 210, 80));
                }
            }
            for (std::size_t eventIndex = 0; eventIndex < document.data().events.size(); ++eventIndex)
            {
                const BoxEvent& event = document.data().events[eventIndex];
                const float x = timeToX(rulerMin.x, event.timeMs);
                drawList->AddLine(ImVec2(x, rulerMin.y + 4.0f), ImVec2(x, rulerMax.y),
                                  session->selectedEvent == static_cast<int>(eventIndex)
                                      ? IM_COL32(255, 235, 80, 255)
                                      : IM_COL32(240, 180, 60, 220), 2.0f);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const int time = static_cast<int>(std::lround(
                    (ImGui::GetIO().MousePos.x - rulerMin.x) / trackWidth * documentDuration));
                session->setPlayheadMs(std::clamp(time, 0, std::max(0, documentDuration - 1)));
                session->selectedTrack = -1;
                session->selectedKey = -1;
                session->selectedEvent = -1;
            }
            drawList->AddLine(ImVec2(playheadX, rulerMin.y), ImVec2(playheadX, rulerMax.y), IM_COL32(255, 70, 70, 255),
                              1.0f);
            drawList->AddTriangleFilled(ImVec2(playheadX - 4.5f, rulerMin.y), ImVec2(playheadX + 4.5f, rulerMin.y),
                                        ImVec2(playheadX, rulerMin.y + 7.0f), IM_COL32(255, 70, 70, 255));
            drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(48, 16, 16, 255));
            drawList->AddRect(badgeMin, badgeMax, IM_COL32(255, 90, 90, 255));
            drawList->AddText(ImVec2(badgeMin.x + 4.0f, badgeMin.y + 1.0f), IM_COL32(255, 228, 228, 255),
                              playheadLabel.c_str());

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
                ImGui::InvisibleButton("BoxKeyTimelineRow", ImVec2(trackWidth, 30.0f),
                                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                const ImVec2 rowMax = ImGui::GetItemRectMax();
                const bool rowHovered = ImGui::IsItemHovered();
                ImGui::PopID();
                drawList->AddRectFilled(rowMin, rowMax, IM_COL32(31, 34, 40, 255));
                const float rowCenterY = (rowMin.y + rowMax.y) * 0.5f;
                for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex)
                {
                    const BoxKey& key = track.keys[keyIndex];
                    if (!key.box)
                        continue;
                    const float x = timeToX(rowMin.x, key.timeMs);
                    const std::int32_t endTime = keyIndex + 1 < track.keys.size() ? track.keys[keyIndex + 1].timeMs
                                                                                    : documentDuration;
                    const float endX = timeToX(rowMin.x, endTime);
                    const ImU32 rangeColor = track.kind == "attack" ? IM_COL32(190, 50, 50, 72)
                                          : track.kind == "hitbox"  ? IM_COL32(200, 150, 30, 72)
                                                                    : IM_COL32(50, 105, 190, 72);
                    drawList->AddRectFilled(ImVec2(x, rowCenterY - 3.0f), ImVec2(endX, rowCenterY + 3.0f), rangeColor);
                }
                const float rowPlayheadX = timeToX(rowMin.x, session->playheadMs());
                drawList->AddLine(ImVec2(rowPlayheadX, rowMin.y), ImVec2(rowPlayheadX, rowMax.y),
                                  IM_COL32(255, 70, 70, 255), 1.0f);
                for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex)
                {
                    const BoxKey& key = track.keys[keyIndex];
                    const float x = timeToX(rowMin.x, key.timeMs);
                    const bool selected = session->selectedTrack == static_cast<int>(trackIndex) &&
                                          session->selectedKey == static_cast<int>(keyIndex);
                    const ImU32 keyColor = selected ? IM_COL32(255, 235, 100, 255)
                                                     : (key.box ? IM_COL32(230, 235, 245, 255)
                                                                : IM_COL32(165, 175, 190, 255));
                    drawDiamond(*drawList, ImVec2(x, rowCenterY), selected ? 7.0f : 5.5f, keyColor, key.box.has_value());
                }

                int hitKey = -1;
                if (rowHovered)
                {
                    const float centerY = (rowMin.y + rowMax.y) * 0.5f;
                    for (int keyIndex = static_cast<int>(track.keys.size()) - 1; keyIndex >= 0; --keyIndex)
                    {
                        const float x = timeToX(rowMin.x, track.keys[keyIndex].timeMs);
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
                            (ImGui::GetIO().MousePos.x - rowMin.x) / trackWidth * documentDuration));
                        document.beginEditTransaction();
                    }
                    else
                    {
                        session->selectedKey = -1;
                        const int time = static_cast<int>(std::lround(
                            (ImGui::GetIO().MousePos.x - rowMin.x) / trackWidth * documentDuration));
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
                        (ImGui::GetIO().MousePos.x - rowMin.x) / trackWidth * documentDuration));
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
