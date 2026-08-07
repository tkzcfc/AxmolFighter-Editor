#include "combat/BoxEditor.h"

#include "2d/Sprite.h"
#include "2d/SpriteFrame.h"
#include "animation/AnimationPreviewController.h"
#include "asset_browser/AssetDragPayload.h"
#include "asset_browser/AssetKind.h"
#include "combat/BoxEditorSessionService.h"
#include "combat/CombatBoxProjection.h"
#include "core/EditorContext.h"
#include "core/ProjectSettings.h"
#include "documents/BoxDocument.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>

namespace editor
{
namespace
{
std::array<ImVec2, 4> spriteFrameUvs(ax::SpriteFrame* frame)
{
    std::array<ImVec2, 4> uvs{};
    if (!frame || !frame->getTexture())
        return uvs;
    const ax::Rect rect = frame->getRectInPixels();
    const float textureWidth = static_cast<float>(frame->getTexture()->getPixelsWide());
    const float textureHeight = static_cast<float>(frame->getTexture()->getPixelsHigh());
    float width = rect.size.width;
    float height = rect.size.height;
    if (frame->isRotated())
        std::swap(width, height);
    const float left = rect.origin.x / textureWidth;
    const float right = (rect.origin.x + width) / textureWidth;
    const float top = rect.origin.y / textureHeight;
    const float bottom = (rect.origin.y + height) / textureHeight;
    if (frame->isRotated())
        uvs = {ImVec2(right, top), ImVec2(right, bottom), ImVec2(left, bottom), ImVec2(left, top)};
    else
        uvs = {ImVec2(left, top), ImVec2(right, top), ImVec2(right, bottom), ImVec2(left, bottom)};
    return uvs;
}

ImVec2 transformFramePoint(const ImVec2& local, const AnimationFrameData& frame)
{
    const float radians = frame.rotation * 3.14159265358979323846f / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float x = local.x * frame.scale.x;
    const float y = local.y * frame.scale.y;
    return {frame.offset.x + x * cosine - y * sine, frame.offset.y + x * sine + y * cosine};
}

std::array<ImVec2, 4> frameWorldCorners(const AnimationFrameData& frame, const ax::Size& sourceSize)
{
    const float left   = -frame.anchor.x * sourceSize.width;
    const float right  = (1.0f - frame.anchor.x) * sourceSize.width;
    const float bottom = -frame.anchor.y * sourceSize.height;
    const float top    = (1.0f - frame.anchor.y) * sourceSize.height;
    return {transformFramePoint({left, top}, frame), transformFramePoint({right, top}, frame),
            transformFramePoint({right, bottom}, frame), transformFramePoint({left, bottom}, frame)};
}

bool pointInRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
{
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

std::int32_t rounded(float value)
{
    return static_cast<std::int32_t>(std::lround(value));
}

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

std::string frameHint(const BoxEditorSessionService& session, std::int32_t timeMs, std::int32_t durationMs)
{
    if (!session.preview().isAni() || session.preview().frameBoundaries().size() < 2 || durationMs <= 0)
        return "Frame —";
    const auto& boundaries = session.preview().frameBoundaries();
    const auto it = std::upper_bound(boundaries.begin(), boundaries.end(), timeMs);
    const std::size_t index = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, (it - boundaries.begin()) - 1));
    return "Frame " + std::to_string(std::min(index + 1, boundaries.size() - 1));
}

ImU32 boxColor(const BoxTrack& track, bool fill)
{
    if (track.kind == "attack")
        return fill ? IM_COL32(235, 55, 55, 72) : IM_COL32(255, 75, 75, 235);
    if (track.kind == "hitbox")
        return fill ? IM_COL32(240, 180, 40, 72) : IM_COL32(255, 200, 60, 235);
    // damage
    return fill ? IM_COL32(55, 125, 240, 72) : IM_COL32(90, 160, 255, 235);
}

ImU32 withAlphaMultiplier(ImU32 color, float multiplier)
{
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    rgba.w *= std::clamp(multiplier, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(rgba);
}
}  // namespace

void BoxEditor::reset()
{
    m_documentPath.clear();
    m_viewState = {};
    m_dragging = false;
    m_dragTrack = -1;
    m_dragKey = -1;
    m_resizingXNegative = false;
    m_resizingZNegative = false;
}

void BoxEditor::drawContent(EditorContext& context, BoxDocument& document)
{
    auto session = context.services().get<BoxEditorSessionService>();
    if (!session)
    {
        ImGui::TextDisabled("Box editor session is not available.");
        return;
    }

    const std::string path = document.path().generic_string();
    if (m_documentPath != path)
    {
        reset();
        m_documentPath = path;
    }
    session->bind(document, context.settings());

    drawPreviewControls(context, document);
    const float canvasHeight = std::max(120.0f, ImGui::GetContentRegionAvail().y);
    drawCanvas(context, document, canvasHeight);
}

void BoxEditor::drawPreviewControls(EditorContext& context, BoxDocument& document)
{
    auto session = context.services().get<BoxEditorSessionService>();
    if (!session)
        return;
    const AnimationPreviewController& preview = session->preview();
    if (ImGui::Button("|<"))
        session->setPlayheadMs(0);
    ImGui::SameLine();
    if (ImGui::Button(session->isPlaying() ? "Pause" : "Play"))
        session->setPlaying(!session->isPlaying());
    ImGui::SameLine();
    if (ImGui::Button(">|"))
        session->setPlayheadMs(std::max(0, document.data().duration - 1));
    ImGui::SameLine();
    ImGui::Text("Time: %d / %d ms", session->playheadMs(), document.data().duration);
    ImGui::SameLine();
    if (preview.isAni())
        ImGui::TextDisabled("Frame %zu", session->preview().frameIndex() + 1);
    else if (preview.isSpine() && preview.isLoaded())
        ImGui::TextDisabled("Spine %d / %d ms", preview.timeMs(), preview.durationMs());
    else
        ImGui::TextDisabled("No preview");
}

void BoxEditor::drawCanvas(EditorContext& context, BoxDocument& document, float height)
{
    auto session = context.services().get<BoxEditorSessionService>();
    if (!session)
        return;
    if (!ImGui::BeginChild("BoxCanvas", ImVec2(0.0f, height), true,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::EndChild();
        return;
    }

    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(1.0f, canvasSize.x);
    canvasSize.y = std::max(1.0f, canvasSize.y);
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("BoxCanvasHit", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    SceneCanvasFrame canvas{canvasPos,
                            {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                            canvasSize,
                            {canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f},
                            m_viewState.pan,
                            m_viewState.zoom};
    ImGuiIO& io = ImGui::GetIO();
    const bool leftPan = hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    handleSceneCanvasNavigation(io.MousePos, io.MouseWheel, io.MouseDelta, hovered, leftPan,
                                m_viewState, canvas);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvas.canvasPos, canvas.canvasEnd, true);
    drawList->AddRectFilled(canvas.canvasPos, canvas.canvasEnd, IM_COL32(28, 31, 36, 255));
    drawSceneCanvasGrid(*drawList, canvas);

    AnimationPreviewController& preview = session->preview();
    if (ax::SpriteFrame* frame = preview.currentFrame())
    {
        if (const AnimationFrameData* frameData = preview.currentFrameData())
        {
            const auto corners = frameWorldCorners(*frameData, frame->getOriginalSizeInPixels());
            std::array<ImVec2, 4> screenCorners;
            for (std::size_t i = 0; i < corners.size(); ++i)
                screenCorners[i] = sceneWorldToScreen(canvas, corners[i]);
            const auto uvs = spriteFrameUvs(frame);
            drawList->AddImageQuad(reinterpret_cast<ImTextureID>(frame->getTexture()), screenCorners[0],
                                   screenCorners[1], screenCorners[2], screenCorners[3], uvs[0], uvs[1], uvs[2],
                                   uvs[3], IM_COL32_WHITE);
        }
    }
    else if (preview.isSpine())
    {
        preview.renderSpine(canvas);
        if (ax::Sprite* sprite = preview.spineRenderSprite(); sprite && sprite->getTexture())
        {
            const float uMax = preview.spineRenderTextureCapacityWidth() > 0
                                   ? std::clamp(static_cast<float>(preview.spineRenderTextureRequestedWidth()) /
                                                    static_cast<float>(preview.spineRenderTextureCapacityWidth()),
                                                0.0f, 1.0f)
                                   : 1.0f;
            const float vMax = preview.spineRenderTextureCapacityHeight() > 0
                                   ? std::clamp(static_cast<float>(preview.spineRenderTextureRequestedHeight()) /
                                                    static_cast<float>(preview.spineRenderTextureCapacityHeight()),
                                                0.0f, 1.0f)
                                   : 1.0f;
            const ImVec2 uv0(0.0f, sprite->isFlippedY() ? vMax : 0.0f);
            const ImVec2 uv1(uMax, sprite->isFlippedY() ? 0.0f : vMax);
            drawList->AddImage(reinterpret_cast<ImTextureID>(sprite->getTexture()), canvas.canvasPos, canvas.canvasEnd,
                               uv0, uv1, IM_COL32_WHITE);
        }
        if (!session->previewError().empty())
        {
            drawList->AddText(ImVec2(canvas.canvasPos.x + 12.0f, canvas.canvasPos.y + 12.0f),
                              IM_COL32(242, 115, 89, 255), session->previewError().c_str());
        }
    }
    else if (!session->previewError().empty())
    {
        drawList->AddText(ImVec2(canvas.canvasPos.x + 12.0f, canvas.canvasPos.y + 12.0f),
                          IM_COL32(242, 115, 89, 255), session->previewError().c_str());
    }

    const int selectedTrackIndex = session->selectedTrack;
    const int selectedKeyIndex = session->selectedKey;
    const BoxKey* editableKey = nullptr;
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(document.data().tracks.size()))
    {
        const BoxTrack& selectedTrack = document.data().tracks[selectedTrackIndex];
        if (selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(selectedTrack.keys.size()))
        {
            const BoxKey& selectedKey = selectedTrack.keys[selectedKeyIndex];
            if (selectedKey.box && resolveBoxKeyAt(selectedTrack, session->playheadMs()) == &selectedKey)
                editableKey = &selectedKey;
        }
    }

    for (std::size_t trackIndex = 0; trackIndex < document.data().tracks.size(); ++trackIndex)
    {
        const BoxTrack& track = document.data().tracks[trackIndex];
        const BoxKey* key = resolveBoxKeyAt(track, session->playheadMs());
        if (!key || !key->box)
            continue;
        const std::size_t keyIndex = static_cast<std::size_t>(key - track.keys.data());
        const bool selected = editableKey == key && selectedTrackIndex == static_cast<int>(trackIndex) &&
                              selectedKeyIndex == static_cast<int>(keyIndex);
        const float alpha = selected ? 1.0f : 0.25f;
        drawCombatBox(*drawList, *key->box,
                      [&canvas](const ImVec2& point) { return sceneWorldToScreen(canvas, point); },
                      withAlphaMultiplier(boxColor(track, false), alpha),
                      withAlphaMultiplier(boxColor(track, true), alpha), selected);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_dragging && editableKey)
    {
        const ProjectedCombatBox projected = projectCombatBox(*editableKey->box);
        const std::array<ImVec2, 4> corners = {
            sceneWorldToScreen(canvas, projected.FL), sceneWorldToScreen(canvas, projected.FR),
            sceneWorldToScreen(canvas, projected.FRT), sceneWorldToScreen(canvas, projected.FLT)};
        ImVec2 min = corners[0];
        ImVec2 max = corners[0];
        for (const ImVec2& corner : corners)
        {
            min.x = std::min(min.x, corner.x);
            min.y = std::min(min.y, corner.y);
            max.x = std::max(max.x, corner.x);
            max.y = std::max(max.y, corner.y);
        }

        if (pointInRect(io.MousePos, min, max))
        {
            m_dragging = true;
            m_dragTrack = selectedTrackIndex;
            m_dragKey = selectedKeyIndex;
            m_resizingX = std::abs(io.MousePos.x - max.x) <= 8.0f;
            m_resizingZ = std::abs(io.MousePos.y - min.y) <= 8.0f;
            m_resizingXNegative = std::abs(io.MousePos.x - min.x) <= 8.0f;
            m_resizingZNegative = std::abs(io.MousePos.y - max.y) <= 8.0f;
            m_dragStartWorld = sceneScreenToWorld(canvas, io.MousePos);
            m_dragOriginalBox = *editableKey->box;
            document.beginEditTransaction();
        }
    }

    if (m_dragging && m_dragTrack >= 0 && m_dragKey >= 0 &&
        m_dragTrack < static_cast<int>(document.data().tracks.size()) &&
        m_dragKey < static_cast<int>(document.data().tracks[m_dragTrack].keys.size()) &&
        document.data().tracks[m_dragTrack].keys[m_dragKey].box)
    {
        const ImVec2 world = sceneScreenToWorld(canvas, io.MousePos);
        const CombatBox original = m_dragOriginalBox;
        CombatBox& box = *document.data().tracks[m_dragTrack].keys[m_dragKey].box;
        const std::int32_t dx = rounded(world.x - m_dragStartWorld.x);
        const std::int32_t dz = rounded(world.y - m_dragStartWorld.y);
        if (m_resizingX)
            box.size.x = std::max<std::int32_t>(1, original.size.x + dx);
        else if (m_resizingXNegative)
        {
            box.pos.x = original.pos.x + dx;
            box.size.x = std::max<std::int32_t>(1, original.size.x - dx);
        }
        if (m_resizingZ)
            box.size.z = std::max<std::int32_t>(1, original.size.z + dz);
        else if (m_resizingZNegative)
        {
            box.pos.z = original.pos.z + dz;
            box.size.z = std::max<std::int32_t>(1, original.size.z - dz);
        }
        if (!m_resizingX && !m_resizingXNegative && !m_resizingZ && !m_resizingZNegative)
        {
            box.pos.x = original.pos.x + dx;
            box.pos.z = original.pos.z + dz;
        }
        document.markDirty();
    }
    if (m_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        document.commitEditTransaction();
        m_dragging = false;
        m_dragTrack = -1;
        m_dragKey = -1;
        m_resizingX = false;
        m_resizingZ = false;
        m_resizingXNegative = false;
        m_resizingZNegative = false;
    }

    drawList->PopClipRect();
    ImGui::EndChild();
}

void BoxEditor::drawInspector(EditorContext& context, BoxDocument& document)
{
    auto session = context.services().get<BoxEditorSessionService>();
    if (!session)
        return;
    ImGui::TextUnformatted("Combat Box");
    ImGui::Separator();

    int duration = document.data().duration;
    if (ImGui::InputInt("Duration (ms)", &duration))
    {
        document.data().duration = std::max(0, duration);
        document.normalizeToDuration();
        document.markDirty();
        session->setPlayheadMs(session->playheadMs());
    }
    ImGui::SeparatorText("Preview");
    BoxPreviewData& previewData = document.data().preview;
    const char* previewKinds[] = {"None", "ANI", "Spine"};
    int previewKindIndex = previewData.kind == BoxPreviewKind::Ani ? 1 : previewData.kind == BoxPreviewKind::Spine ? 2 : 0;
    if (ImGui::Combo("Type", &previewKindIndex, previewKinds, IM_ARRAYSIZE(previewKinds)))
    {
        document.beginEditTransaction();
        previewData.kind = previewKindIndex == 1 ? BoxPreviewKind::Ani
                           : previewKindIndex == 2 ? BoxPreviewKind::Spine
                                                   : BoxPreviewKind::None;
        previewData.path.clear();
        previewData.atlasPath.clear();
        previewData.animation.clear();
        document.markDirty();
        document.commitEditTransaction();
        session->bind(document, context.settings());
        session->setPlayheadMs(session->playheadMs());
    }

    if (ImGui::InputText("Asset Path", &previewData.path))
    {
        document.markDirty();
        session->bind(document, context.settings());
        session->setPlayheadMs(session->playheadMs());
    }
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayloadType);
            payload && payload->DataSize == sizeof(AssetDragPayload))
        {
            const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
            if (asset.kind == AssetKind::FrameAnimation || asset.kind == AssetKind::Spine)
            {
                document.beginEditTransaction();
                previewData.kind = asset.kind == AssetKind::Spine ? BoxPreviewKind::Spine : BoxPreviewKind::Ani;
                previewData.path = asset.relativePath;
                previewData.atlasPath = asset.kind == AssetKind::Spine ? asset.atlasPath : "";
                previewData.animation.clear();
                document.markDirty();
                document.commitEditTransaction();
                session->bind(document, context.settings());
                if (previewData.kind == BoxPreviewKind::Spine && !session->preview().spineAnimations().empty())
                {
                    document.beginEditTransaction();
                    previewData.animation = session->preview().spineAnimations().front();
                    document.markDirty();
                    document.commitEditTransaction();
                    session->bind(document, context.settings());
                }
                session->setPlayheadMs(session->playheadMs());
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (previewData.kind == BoxPreviewKind::Spine)
    {
        if (ImGui::InputText("Atlas Path", &previewData.atlasPath))
        {
            document.markDirty();
            session->bind(document, context.settings());
            session->setPlayheadMs(session->playheadMs());
        }
        const std::vector<std::string>& animations = session->preview().spineAnimations();
        const char* animationLabel = previewData.animation.empty() ? "Select animation" : previewData.animation.c_str();
        if (ImGui::BeginCombo("Animation", animationLabel))
        {
            for (const std::string& animation : animations)
            {
                if (ImGui::Selectable(animation.c_str(), animation == previewData.animation))
                {
                    document.beginEditTransaction();
                    previewData.animation = animation;
                    document.markDirty();
                    document.commitEditTransaction();
                    session->bind(document, context.settings());
                    session->setPlayheadMs(session->playheadMs());
                }
            }
            ImGui::EndCombo();
        }
    }
    const bool canMatchPreviewDuration = session->preview().isLoaded() && session->preview().durationMs() > 0;
    ImGui::BeginDisabled(!canMatchPreviewDuration);
    if (ImGui::Button("Match Box Duration to Preview"))
    {
        document.beginEditTransaction();
        document.data().duration = session->preview().durationMs();
        document.normalizeToDuration();
        document.markDirty();
        document.commitEditTransaction();
        session->setPlayheadMs(session->playheadMs());
    }
    ImGui::EndDisabled();
    if (!session->previewError().empty())
        ImGui::TextColored(ImVec4(0.95f, 0.32f, 0.27f, 1.0f), "%s", session->previewError().c_str());

    if (ImGui::Button("Add Track"))
    {
        document.data().tracks.push_back({"track" + std::to_string(document.data().tracks.size() + 1), "hitbox", {}});
        document.markDirty();
        session->selectedTrack = static_cast<int>(document.data().tracks.size() - 1);
        session->selectedKey = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Event"))
    {
        document.data().events.push_back({session->playheadMs(), "event", ""});
        document.markDirty();
        session->selectedEvent = static_cast<int>(document.data().events.size() - 1);
    }

    ImGui::SeparatorText("Tracks");
    for (std::size_t index = 0; index < document.data().tracks.size(); ++index)
    {
        BoxTrack& track = document.data().tracks[index];
        ImGui::PushID(static_cast<int>(index));
        const bool selected = session->selectedTrack == static_cast<int>(index);
        if (ImGui::Selectable(track.name.c_str(), selected))
        {
            session->selectedTrack = static_cast<int>(index);
            session->selectedKey = -1;
            session->selectedEvent = -1;
        }
        ImGui::PopID();
    }

    if (session->selectedTrack >= 0 && session->selectedTrack < static_cast<int>(document.data().tracks.size()))
    {
        BoxTrack& track = document.data().tracks[session->selectedTrack];
        ImGui::SeparatorText("Selected Track");
        if (ImGui::InputText("Name", &track.name))
            document.markDirty();
        const char* kinds[] = {"attack", "damage", "hitbox"};
        int kindIndex       = 2;
        if (track.kind == "attack")
            kindIndex = 0;
        else if (track.kind == "damage")
            kindIndex = 1;
        if (ImGui::Combo("Kind", &kindIndex, kinds, 3))
        {
            track.kind = kinds[kindIndex];
            document.markDirty();
        }
        if (ImGui::Button("Duplicate Track"))
        {
            BoxTrack copy = track;
            copy.name += " Copy";
            document.data().tracks.insert(document.data().tracks.begin() + session->selectedTrack + 1, copy);
            document.markDirty();
            ++session->selectedTrack;
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy Track"))
            session->setTrackClipboard(track);
        ImGui::SameLine();
        if (ImGui::Button("Paste Track") && session->hasTrackClipboard())
        {
            BoxTrack copy = session->trackClipboard();
            copy.name += " Copy";
            track = copy;
            document.normalizeKeys();
            document.markDirty();
        }

        ImGui::Text("Keys: %zu", track.keys.size());
        ImGui::SameLine();
        if (ImGui::Button("Add Key at Playhead") && document.data().duration > 0)
        {
            const std::int32_t timeMs = session->playheadMs();
            const BoxKey* previous = keyBefore(track, timeMs);
            BoxKey key{timeMs, previous && previous->box ? previous->box : std::optional<CombatBox>(defaultCombatBox())};
            session->selectedKey = static_cast<int>(upsertKey(track, std::move(key)));
            document.markDirty();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("##ShiftKeysMs", &m_shiftKeysMs);
        ImGui::SameLine();
        if (ImGui::Button("Shift Keys") && !track.keys.empty())
        {
            const std::int32_t minimumDelta = -track.keys.front().timeMs;
            const std::int32_t maximumDelta = document.data().duration - 1 - track.keys.back().timeMs;
            const std::int32_t delta = std::clamp(m_shiftKeysMs, minimumDelta, maximumDelta);
            for (BoxKey& key : track.keys)
                key.timeMs += delta;
            if (session->selectedKey >= 0 && session->selectedKey < static_cast<int>(track.keys.size()))
                session->setPlayheadMs(track.keys[session->selectedKey].timeMs);
            m_shiftKeysMs = 0;
            document.markDirty();
        }

        if (ImGui::Button("Paste Key at Playhead") && session->hasKeyClipboard() && document.data().duration > 0)
        {
            BoxKey key = session->keyClipboard();
            key.timeMs = session->playheadMs();
            session->selectedKey = static_cast<int>(upsertKey(track, std::move(key)));
            document.markDirty();
        }

        for (std::size_t index = 0; index < track.keys.size(); ++index)
        {
            ImGui::PushID(static_cast<int>(index));
            const BoxKey& key = track.keys[index];
            const std::string label = std::string(key.box ? "◆ " : "◇ ") + std::to_string(key.timeMs) + " ms";
            if (ImGui::Selectable(label.c_str(), session->selectedKey == static_cast<int>(index)))
            {
                session->selectedKey = static_cast<int>(index);
                session->selectedEvent = -1;
                session->setPlayheadMs(key.timeMs);
            }
            ImGui::PopID();
        }

        if (session->selectedKey >= 0 && session->selectedKey < static_cast<int>(track.keys.size()))
        {
            BoxKey& key = track.keys[session->selectedKey];
            ImGui::SeparatorText("Selected Key");
            int timeMs = key.timeMs;
            if (ImGui::InputInt("Time (ms)", &timeMs))
            {
                BoxKey moved = key;
                moved.timeMs = std::clamp(timeMs, 0, std::max(0, document.data().duration - 1));
                track.keys.erase(track.keys.begin() + session->selectedKey);
                session->selectedKey = static_cast<int>(upsertKey(track, std::move(moved)));
                document.markDirty();
                session->setPlayheadMs(track.keys[session->selectedKey].timeMs);
                return;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", frameHint(*session, key.timeMs, document.data().duration).c_str());
            if (key.box)
            {
                if (ImGui::Button("Set Null"))
                {
                    key.box.reset();
                    document.markDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Copy Key"))
                    session->setKeyClipboard(key);
                CombatBox& box = *key.box;
                int values[3] = {box.pos.x, box.pos.y, box.pos.z};
                if (ImGui::DragInt3("Position", values))
                {
                    box.pos = {values[0], values[1], values[2]};
                    document.markDirty();
                }
                int sizes[3] = {box.size.x, box.size.y, box.size.z};
                if (ImGui::DragInt3("Size", sizes))
                {
                    box.size = {std::max(1, sizes[0]), std::max(1, sizes[1]), std::max(1, sizes[2])};
                    document.markDirty();
                }
            }
            else
            {
                if (ImGui::Button("Create Default Box"))
                {
                    key.box = defaultCombatBox();
                    document.markDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Copy Null Key"))
                    session->setKeyClipboard(key);
            }
        }
    }

    ImGui::SeparatorText("Events");
    for (std::size_t index = 0; index < document.data().events.size(); ++index)
    {
        BoxEvent& event = document.data().events[index];
        ImGui::PushID(static_cast<int>(index));
        const std::string label = std::to_string(event.timeMs) + " ms  " + event.type;
        if (ImGui::Selectable(label.c_str(), session->selectedEvent == static_cast<int>(index)))
        {
            session->selectedEvent = static_cast<int>(index);
            session->selectedTrack = -1;
            session->selectedKey = -1;
        }
        ImGui::PopID();
    }
    if (session->selectedEvent >= 0 && session->selectedEvent < static_cast<int>(document.data().events.size()))
    {
        BoxEvent& event = document.data().events[session->selectedEvent];
        int time = event.timeMs;
        if (ImGui::InputInt("Event Time (ms)", &time))
        {
            event.timeMs = std::clamp(time, 0, document.data().duration);
            document.markDirty();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", frameHint(*session, event.timeMs, document.data().duration).c_str());
        if (ImGui::InputText("Event Type", &event.type))
            document.markDirty();
        if (ImGui::InputText("Event Value", &event.value))
            document.markDirty();
    }
}
}  // namespace editor
