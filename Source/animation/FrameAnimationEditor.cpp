#include "animation/FrameAnimationEditor.h"

#include "2d/Sprite.h"
#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"
#include "ImGui/ImGuiPresenter.h"
#include "asset_browser/AssetDragPayload.h"
#include "core/EditorContext.h"
#include "documents/AniDocument.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"
#include "base/Director.h"
#include "scene/canvas/SceneCanvasToolbarContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <tuple>

namespace editor
{
namespace
{
constexpr float kAnimationToolbarWidth  = 44.0f;
constexpr const char* kFrameToolId      = "animation.frame";
constexpr const char* kPanToolId        = "animation.pan";
constexpr const char* kIconMousePointer = "\xef\x89\x85";  // fa-mouse-pointer
constexpr const char* kIconHandPaper    = "\xef\x89\x96";  // fa-hand-paper

AnimationFrameData frameFromPayload(const AssetDragPayload& payload)
{
    AnimationFrameData frame;
    if (payload.kind == AssetKind::SpriteFrame)
    {
        frame.sourceType = AnimationFrameSourceType::SpriteFrame;
        frame.atlasPath  = payload.atlasPath;
        frame.frameName  = payload.frameName;
    }
    else
    {
        frame.path = payload.relativePath;
    }
    return frame;
}

bool isSupportedFrameAsset(const AssetDragPayload& payload)
{
    return payload.kind == AssetKind::Texture || payload.kind == AssetKind::SpriteFrame;
}

bool validPreviewVariableName(const std::string& name)
{
    return !name.empty() && name.find('{') == std::string::npos && name.find('}') == std::string::npos;
}

std::int64_t animationDurationMs(const FrameAnimationData& animation)
{
    std::int64_t result = 0;
    for (const AnimationFrameData& frame : animation.frames)
        result += std::max(1, frame.delayMs);
    return result;
}

std::array<ImVec2, 4> spriteFrameUvs(ax::SpriteFrame* frame)
{
    std::array<ImVec2, 4> uvs{};
    if (!frame || !frame->getTexture())
        return uvs;
    const ax::Rect rect     = frame->getRectInPixels();
    const float atlasWidth  = static_cast<float>(frame->getTexture()->getPixelsWide());
    const float atlasHeight = static_cast<float>(frame->getTexture()->getPixelsHigh());
    float width             = rect.size.width;
    float height            = rect.size.height;
    if (frame->isRotated())
        std::swap(width, height);
#if AX_FIX_ARTIFACTS_BY_STRECHING_TEXEL
    const float left   = (2.0f * rect.origin.x + 1.0f) / (2.0f * atlasWidth);
    const float right  = left + (width * 2.0f - 2.0f) / (2.0f * atlasWidth);
    const float top    = (2.0f * rect.origin.y + 1.0f) / (2.0f * atlasHeight);
    const float bottom = top + (height * 2.0f - 2.0f) / (2.0f * atlasHeight);
#else
    const float left   = rect.origin.x / atlasWidth;
    const float right  = (rect.origin.x + width) / atlasWidth;
    const float top    = rect.origin.y / atlasHeight;
    const float bottom = (rect.origin.y + height) / atlasHeight;
#endif
    if (frame->isRotated())
        uvs = {ImVec2(right, top), ImVec2(right, bottom), ImVec2(left, bottom), ImVec2(left, top)};
    else
        uvs = {ImVec2(left, top), ImVec2(right, top), ImVec2(right, bottom), ImVec2(left, bottom)};
    return uvs;
}

ImVec2 transformFramePoint(const ImVec2& local, const AnimationFrameData& frame)
{
    const float radians = frame.rotation * 3.14159265358979323846f / 180.0f;
    const float cosine  = std::cos(radians);
    const float sine    = std::sin(radians);
    const float x       = local.x * frame.scale.x;
    const float y       = local.y * frame.scale.y;
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

bool pointInQuad(const std::array<ImVec2, 4>& points, const ImVec2& point)
{
    float sign = 0.0f;
    for (int index = 0; index < 4; ++index)
    {
        const ImVec2& a   = points[index];
        const ImVec2& b   = points[(index + 1) % 4];
        const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
        if (std::abs(cross) < 0.0001f)
            continue;
        if (sign == 0.0f)
            sign = cross;
        else if ((sign > 0.0f) != (cross > 0.0f))
            return false;
    }
    return true;
}

}  // namespace

FrameAnimationEditor::FrameAnimationEditor()
{
    m_sprite = ax::Sprite::create();
    if (m_sprite)
        m_sprite->retain();
}

FrameAnimationEditor::~FrameAnimationEditor()
{
    clearPreviewFrames();
    if (m_sprite)
        m_sprite->release();
}

void FrameAnimationEditor::update(float deltaTime)
{
    if (!m_playing || m_previewFrames.empty() || m_selectedFrame >= m_frameDelaysMs.size())
        return;
    m_frameElapsedMs += std::max(0.0f, deltaTime) * 1000.0f;
    while (m_playing && m_selectedFrame < m_frameDelaysMs.size() &&
           m_frameElapsedMs >= static_cast<float>(std::max(1, m_frameDelaysMs[m_selectedFrame])))
    {
        m_frameElapsedMs -= static_cast<float>(std::max(1, m_frameDelaysMs[m_selectedFrame]));
        if (m_selectedFrame + 1 < m_previewFrames.size())
        {
            ++m_selectedFrame;
        }
        else if (m_currentLoop)
        {
            m_selectedFrame = 0;
        }
        else
        {
            m_playing        = false;
            m_finished       = true;
            m_frameElapsedMs = 0.0f;
        }
        if (m_sprite)
            m_sprite->setSpriteFrame(m_previewFrames[m_selectedFrame]);
    }
}

void FrameAnimationEditor::reset()
{
    clearPreviewFrames();
    m_documentPath.clear();
    m_selectedFrame        = 0;
    m_playing              = false;
    m_finished             = false;
    m_frameElapsedMs       = 0.0f;
    m_seenDocumentRevision = 0;
    m_previewError.clear();
    m_viewState              = {};
    m_activeToolId           = kFrameToolId;
    m_draggingFrameTransform = false;
}

void FrameAnimationEditor::prepare(AniDocument& document)
{
    if (m_documentPath != document.path())
    {
        reset();
        m_documentPath = document.path();
        showSelectedFrame(document);
    }
    else if (m_seenDocumentRevision != document.revision())
    {
        stopPlayback(document);
    }
}

void FrameAnimationEditor::drawContent(EditorContext& context, AniDocument& document)
{
    prepare(document);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && m_selectedFrame > 0)
            setSelectedFrame(document, m_selectedFrame - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && m_selectedFrame + 1 < document.animation().frames.size())
            setSelectedFrame(document, m_selectedFrame + 1);
    }

    drawPreview(context, document);
    m_seenDocumentRevision = document.revision();
}

ax::SpriteFrame* FrameAnimationEditor::resolveFrame(const AniDocument& document,
                                                    const AnimationFrameData& frame,
                                                    std::string& error) const
{
    error.clear();
    if (frame.sourceType == AnimationFrameSourceType::Template)
    {
        std::string resolvedPath;
        if (!document.resolveTemplatePath(frame.templatePath, resolvedPath, error))
            return nullptr;
        ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(resolvedPath);
        if (!texture)
        {
            error = "Unable to load template texture: " + resolvedPath;
            return nullptr;
        }
        return ax::SpriteFrame::createWithTexture(texture,
                                                  ax::Rect(0.0f, 0.0f, static_cast<float>(texture->getPixelsWide()),
                                                           static_cast<float>(texture->getPixelsHigh())));
    }
    if (frame.sourceType == AnimationFrameSourceType::SpriteFrame)
    {
        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(frame.atlasPath))
            cache->addSpriteFramesWithFile(frame.atlasPath);
        ax::SpriteFrame* spriteFrame = cache->getSpriteFrameByName(frame.frameName);
        if (!spriteFrame)
            error = "Unable to load sprite frame: " + frame.frameName;
        return spriteFrame;
    }
    ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(frame.path);
    if (!texture)
    {
        error = "Unable to load texture: " + frame.path;
        return nullptr;
    }
    return ax::SpriteFrame::createWithTexture(texture,
                                              ax::Rect(0.0f, 0.0f, static_cast<float>(texture->getPixelsWide()),
                                                       static_cast<float>(texture->getPixelsHigh())));
}

void FrameAnimationEditor::rebuildPreview(AniDocument& document, bool startPlaying)
{
    clearPreviewFrames();
    m_previewError.clear();
    m_playing                      = false;
    m_finished                     = false;
    m_frameElapsedMs               = 0.0f;
    const FrameAnimationData& data = document.animation();
    for (const AnimationFrameData& frameData : data.frames)
    {
        std::string error;
        ax::SpriteFrame* frame = resolveFrame(document, frameData, error);
        if (!frame)
        {
            m_previewError = error;
            showSelectedFrame(document);
            return;
        }
        frame->retain();
        m_previewFrames.push_back(frame);
        m_frameDelaysMs.push_back(std::max(1, frameData.delayMs));
    }
    if (m_previewFrames.empty())
        return;
    m_currentLoop   = data.loop;
    m_selectedFrame = 0;
    m_playing       = startPlaying;
    if (m_sprite)
        m_sprite->setSpriteFrame(m_previewFrames.front());
}

void FrameAnimationEditor::startPlayback(AniDocument& document)
{
    if (document.animation().frames.empty())
        return;
    if (m_previewFrames.size() != document.animation().frames.size() || !m_previewError.empty())
    {
        rebuildPreview(document, true);
        return;
    }
    if (m_finished)
    {
        m_selectedFrame  = 0;
        m_frameElapsedMs = 0.0f;
        m_finished       = false;
        if (m_sprite)
            m_sprite->setSpriteFrame(m_previewFrames.front());
    }
    m_currentLoop = document.animation().loop;
    m_playing     = true;
}

void FrameAnimationEditor::clearPreviewFrames()
{
    for (ax::SpriteFrame* frame : m_previewFrames)
        frame->release();
    m_previewFrames.clear();
    m_frameDelaysMs.clear();
}

void FrameAnimationEditor::showSelectedFrame(AniDocument& document)
{
    const auto& frames = document.animation().frames;
    if (frames.empty() || !m_sprite)
        return;
    m_selectedFrame = std::min(m_selectedFrame, frames.size() - 1);
    std::string error;
    ax::SpriteFrame* frame = m_previewFrames.size() == frames.size()
                                 ? m_previewFrames[m_selectedFrame]
                                 : resolveFrame(document, frames[m_selectedFrame], error);
    m_previewError         = error;
    if (frame)
        m_sprite->setSpriteFrame(frame);
}

void FrameAnimationEditor::setSelectedFrame(AniDocument& document, std::size_t index)
{
    if (document.animation().frames.empty())
        return;
    m_playing        = false;
    m_finished       = false;
    m_frameElapsedMs = 0.0f;
    m_selectedFrame  = std::min(index, document.animation().frames.size() - 1);
    showSelectedFrame(document);
}

void FrameAnimationEditor::stopPlayback(AniDocument& document)
{
    clearPreviewFrames();
    m_playing        = false;
    m_finished       = false;
    m_frameElapsedMs = 0.0f;
    showSelectedFrame(document);
}

void FrameAnimationEditor::drawPreview(EditorContext& context, AniDocument& document)
{
    (void)context;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float controlsHeight = ImGui::GetFrameHeightWithSpacing();
    const float previewHeight = std::max(1.0f, available.y - controlsHeight);
    drawAnimationCanvas(document, previewHeight);

    const auto& frames   = document.animation().frames;
    const bool hasFrames = !frames.empty();
    ImGui::BeginDisabled(!hasFrames);
    if (ImGui::Button("|<"))
        setSelectedFrame(document, 0);
    ImGui::SameLine();
    if (ImGui::Button("<") && m_selectedFrame > 0)
        setSelectedFrame(document, m_selectedFrame - 1);
    ImGui::SameLine();
    if (ImGui::Button(m_playing ? "Pause" : "Play"))
    {
        if (m_playing)
            m_playing = false;
        else
            startPlayback(document);
    }
    ImGui::SameLine();
    if (ImGui::Button(">") && m_selectedFrame + 1 < frames.size())
        setSelectedFrame(document, m_selectedFrame + 1);
    ImGui::SameLine();
    if (ImGui::Button(">|") && hasFrames)
        setSelectedFrame(document, frames.size() - 1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu", hasFrames ? m_selectedFrame + 1 : 0, frames.size());
}

void FrameAnimationEditor::drawCanvasToolbar(float height)
{
    SceneCanvasToolbarContext toolbar(m_activeToolId);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 21, 25, 230));
    ImGui::BeginChild("AnimationCanvasToolbar", ImVec2(kAnimationToolbarWidth, height), true);
    toolbar.drawToolButton(kFrameToolId, "Frame Transform", kIconMousePointer, 0xf245,
                           "Drag the current frame image offset", "F");
    toolbar.drawToolButton(kPanToolId, "Pan", kIconHandPaper, 0xf256, "Drag the canvas", "P");
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void FrameAnimationEditor::drawAnimationCanvas(AniDocument& document, float height)
{
    drawCanvasToolbar(height);
    ImGui::SameLine();
    if (!ImGui::BeginChild("AnimationCanvas", ImVec2(0.0f, height), true,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::EndChild();
        return;
    }

    ImVec2 canvasSize      = ImGui::GetContentRegionAvail();
    canvasSize.x           = std::max(1.0f, canvasSize.x);
    canvasSize.y           = std::max(1.0f, canvasSize.y);
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("AnimationCanvasHit", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    SceneCanvasFrame canvas{canvasPos,       {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                            canvasSize,      {canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f},
                            m_viewState.pan, m_viewState.zoom};

    ImGuiIO& io             = ImGui::GetIO();
    const bool temporaryPan = hovered && ImGui::IsKeyDown(ImGuiKey_Space);
    const bool leftPan =
        (m_activeToolId == kPanToolId || temporaryPan) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f);
    handleSceneCanvasNavigation(io.MousePos, io.MouseWheel, io.MouseDelta, hovered,
                                ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || leftPan, m_viewState, canvas);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvas.canvasPos, canvas.canvasEnd, true);
    drawList->AddRectFilled(canvas.canvasPos, canvas.canvasEnd, IM_COL32(28, 31, 36, 255));
    drawSceneCanvasGrid(*drawList, canvas);

    if (document.animation().frames.empty())
    {
        const char* text      = "Drop textures or sprite frames here";
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        drawList->AddText({canvas.baseCenter.x - textSize.x * 0.5f, canvas.baseCenter.y - textSize.y * 0.5f},
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
    }
    else if (!m_previewError.empty())
    {
        drawList->AddText({canvas.canvasPos.x + 12.0f, canvas.canvasPos.y + 12.0f}, IM_COL32(242, 115, 89, 255),
                          m_previewError.c_str());
    }
    else if (m_sprite && m_sprite->getSpriteFrame() && m_selectedFrame < document.animation().frames.size())
    {
        AnimationFrameData& frame    = document.animation().frames[m_selectedFrame];
        ax::SpriteFrame* spriteFrame = m_sprite->getSpriteFrame();
        const ax::Size sourceSize    = spriteFrame->getOriginalSizeInPixels();
        const auto worldCorners      = frameWorldCorners(frame, sourceSize);
        std::array<ImVec2, 4> screenCorners;
        for (int index = 0; index < 4; ++index)
            screenCorners[index] = sceneWorldToScreen(canvas, worldCorners[index]);
        const auto uvs = spriteFrameUvs(spriteFrame);
        const ImVec4 tint(frame.color.r, frame.color.g, frame.color.b, frame.color.a);
        drawList->AddImageQuad(reinterpret_cast<ImTextureID>(spriteFrame->getTexture()), screenCorners[0],
                               screenCorners[1], screenCorners[2], screenCorners[3], uvs[0], uvs[1], uvs[2], uvs[3],
                               ImGui::GetColorU32(tint));

        const ImVec2 mouseWorld = sceneScreenToWorld(canvas, io.MousePos);
        if (m_draggingFrameTransform && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            document.commitEditTransaction();
            m_draggingFrameTransform = false;
        }

        if (hovered && m_activeToolId == kFrameToolId && !temporaryPan &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && pointInQuad(screenCorners, io.MousePos))
        {
            pauseForEditing();
            document.beginEditTransaction();
            m_draggingFrameTransform = true;
            m_dragStartWorld         = mouseWorld;
            m_dragStartFrameOffset   = frame.offset;
        }
        if (m_draggingFrameTransform && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            frame.offset = {m_dragStartFrameOffset.x + mouseWorld.x - m_dragStartWorld.x,
                            m_dragStartFrameOffset.y + mouseWorld.y - m_dragStartWorld.y};
            document.markDirty();
        }
    }
    drawList->PopClipRect();
    acceptFrameDrop(document, document.animation().frames.size(), false);
    ImGui::EndChild();
}

void FrameAnimationEditor::pauseForEditing()
{
    m_playing  = false;
    m_finished = false;
}

void FrameAnimationEditor::drawTimeline(EditorContext&, AniDocument& document)
{
    ImGui::SeparatorText("Timeline");
    ImVec2 timelineSize = ImGui::GetContentRegionAvail();
    timelineSize.x      = std::max(1.0f, timelineSize.x);
    timelineSize.y      = std::max(1.0f, timelineSize.y);
    if (ImGui::BeginChild("AnimationTimeline", timelineSize, true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        auto& frames   = document.animation().frames;
        bool reordered = false;
        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            ImGui::PushID(static_cast<int>(index));
            ImGui::BeginGroup();
            std::string error;
            ax::SpriteFrame* previewFrame = resolveFrame(document, frames[index], error);
            if (previewFrame)
                ax::extension::ImGuiPresenter::getInstance()->image(
                    previewFrame, ImVec2(64.0f, 64.0f), true,
                    {frames[index].color.r, frames[index].color.g, frames[index].color.b, frames[index].color.a});
            else
                ImGui::Button("Missing", ImVec2(64.0f, 64.0f));
            if (index == m_selectedFrame)
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                    ImGui::GetColorU32(ImGuiCol_ButtonActive), 2.0f, 0, 2.0f);
            if (ImGui::IsItemClicked())
                setSelectedFrame(document, index);
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("ANI_FRAME_INDEX", &index, sizeof(index));
                ImGui::Text("Move frame %zu", index + 1);
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ANI_FRAME_INDEX");
                    payload && payload->DataSize == sizeof(std::size_t))
                {
                    const std::size_t from = *static_cast<const std::size_t*>(payload->Data);
                    if (from < frames.size() && from != index)
                    {
                        AnimationFrameData moved = std::move(frames[from]);
                        frames.erase(frames.begin() + static_cast<std::ptrdiff_t>(from));
                        const std::size_t target = from < index ? index - 1 : index;
                        frames.insert(frames.begin() + static_cast<std::ptrdiff_t>(target), std::move(moved));
                        m_selectedFrame = target;
                        document.markDirty();
                        stopPlayback(document);
                        reordered = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            acceptFrameDrop(document, index, true);
            ImGui::Text("%zu  %d ms", index + 1, std::max(1, frames[index].delayMs));
            ImGui::EndGroup();
            if (index + 1 < frames.size())
                ImGui::SameLine();
            ImGui::PopID();
            if (reordered)
                break;
        }
        if (!frames.empty() && !reordered)
            ImGui::SameLine();
        if (!reordered)
        {
            ImGui::Selectable("+ Drop\n  Here", false, 0, ImVec2(78.0f, 78.0f));
            acceptFrameDrop(document, frames.size(), false);
        }
    }
    ImGui::EndChild();
}

void FrameAnimationEditor::drawInspector(AniDocument& document)
{
    FrameAnimationData& data = document.animation();
    ImGui::SeparatorText("Animation");
    ImGui::SeparatorText("Preview Variables");
    if (ImGui::Button("Add Variable"))
    {
        std::string key = "var";
        int serial      = 1;
        while (data.previewVars.contains(key))
            key = "var" + std::to_string(serial++);
        data.previewVars.emplace(key, "");
        m_previewVariableKeyEdit.clear();
        m_previewVariableError.clear();
        document.markDirty();
        stopPlayback(document);
    }
    if (!data.previewVars.empty())
    {
        ImGui::BeginChild("AnimationPreviewVariables", ImVec2(0.0f, 120.0f), true);
        for (auto it = data.previewVars.begin(); it != data.previewVars.end();)
        {
            const std::string oldKey = it->first;
            ImGui::PushID(oldKey.c_str());
            if (m_previewVariableKeyEdit != oldKey)
            {
                m_previewVariableKeyEdit   = oldKey;
                m_previewVariableKeyBuffer = oldKey;
            }
            bool commitKey = ImGui::InputText("Key", &m_previewVariableKeyBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
            commitKey |= ImGui::IsItemDeactivatedAfterEdit();
            if (commitKey && m_previewVariableKeyBuffer != oldKey)
            {
                const std::string newKey = m_previewVariableKeyBuffer;
                if (!validPreviewVariableName(newKey))
                    m_previewVariableError = "Variable name cannot be empty or contain braces.";
                else if (data.previewVars.contains(newKey))
                    m_previewVariableError = "Variable name already exists.";
                else
                {
                    const std::string value = std::move(it->second);
                    it                      = data.previewVars.erase(it);
                    it                      = data.previewVars.emplace_hint(it, newKey, value);
                    m_previewVariableKeyEdit.clear();
                    m_previewVariableError.clear();
                    document.markDirty();
                    stopPlayback(document);
                    ImGui::PopID();
                    continue;
                }
            }
            ImGui::SameLine();
            if (ImGui::InputText("Value", &it->second))
            {
                document.markDirty();
                stopPlayback(document);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove"))
            {
                it = data.previewVars.erase(it);
                m_previewVariableKeyEdit.clear();
                m_previewVariableError.clear();
                document.markDirty();
                stopPlayback(document);
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            ++it;
        }
        ImGui::EndChild();
    }
    if (!m_previewVariableError.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", m_previewVariableError.c_str());
    if (ImGui::Checkbox("Loop", &data.loop))
    {
        document.markDirty();
        stopPlayback(document);
    }
    const std::int64_t durationMs = animationDurationMs(data);
    ImGui::TextDisabled("Frames: %zu", data.frames.size());
    ImGui::TextDisabled("Duration: %lld ms (%.3f s)", static_cast<long long>(durationMs),
                        static_cast<double>(durationMs) / 1000.0);
    if (data.frames.empty())
        return;

    m_selectedFrame           = std::min(m_selectedFrame, data.frames.size() - 1);
    AnimationFrameData& frame = data.frames[m_selectedFrame];
    ImGui::SeparatorText("Selected Frame");
    ImGui::Text("Frame %zu", m_selectedFrame + 1);
    const char* sourceTypes[] = {"Texture", "SpriteFrame", "Template"};
    int sourceType            = frame.sourceType == AnimationFrameSourceType::SpriteFrame ? 1
                                : frame.sourceType == AnimationFrameSourceType::Template  ? 2
                                                                                          : 0;
    if (ImGui::Combo("Source Type", &sourceType, sourceTypes, 3))
    {
        frame.sourceType = sourceType == 1   ? AnimationFrameSourceType::SpriteFrame
                           : sourceType == 2 ? AnimationFrameSourceType::Template
                                             : AnimationFrameSourceType::Texture;
        document.markDirty();
        stopPlayback(document);
    }
    bool sourceChanged = false;
    if (frame.sourceType == AnimationFrameSourceType::SpriteFrame)
    {
        sourceChanged |= ImGui::InputText("Atlas", &frame.atlasPath);
        sourceChanged |= ImGui::InputText("Frame", &frame.frameName);
    }
    else if (frame.sourceType == AnimationFrameSourceType::Template)
    {
        sourceChanged |= ImGui::InputText("Image Template", &frame.templatePath);
        std::string resolvedPath;
        std::string resolveError;
        if (document.resolveTemplatePath(frame.templatePath, resolvedPath, resolveError))
            ImGui::TextDisabled("Resolved: %s", resolvedPath.c_str());
        else if (!frame.templatePath.empty())
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", resolveError.c_str());
    }
    else
        sourceChanged |= ImGui::InputText("Texture", &frame.path);
    if (sourceChanged)
    {
        document.markDirty();
        stopPlayback(document);
    }

    int delayMs = frame.delayMs;
    if (ImGui::InputInt("Delay (ms)", &delayMs))
    {
        frame.delayMs = std::max(1, delayMs);
        document.markDirty();
        stopPlayback(document);
    }
    float offset[2] = {frame.offset.x, frame.offset.y};
    if (ImGui::InputFloat2("Offset", offset))
    {
        pauseForEditing();
        frame.offset = {offset[0], offset[1]};
        document.markDirty();
    }
    float anchor[2] = {frame.anchor.x, frame.anchor.y};
    if (ImGui::InputFloat2("Anchor", anchor))
    {
        pauseForEditing();
        frame.anchor = {anchor[0], anchor[1]};
        document.markDirty();
    }
    if (ImGui::Button("Center##Anchor"))
    {
        pauseForEditing();
        frame.anchor = {0.5f, 0.5f};
        document.markDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Top-Left##Anchor"))
    {
        pauseForEditing();
        frame.anchor = {0.0f, 1.0f};
        document.markDirty();
    }
    float scale[2] = {frame.scale.x, frame.scale.y};
    if (ImGui::InputFloat2("Scale", scale))
    {
        pauseForEditing();
        frame.scale = {scale[0], scale[1]};
        document.markDirty();
    }
    if (ImGui::InputFloat("Rotation", &frame.rotation))
    {
        pauseForEditing();
        document.markDirty();
    }
    float color[4] = {frame.color.r, frame.color.g, frame.color.b, frame.color.a};
    if (ImGui::ColorEdit4("Color", color))
    {
        pauseForEditing();
        frame.color = {color[0], color[1], color[2], color[3]};
        document.markDirty();
    }

    if (ImGui::Button("Duplicate"))
    {
        if (document.duplicateFrame(m_selectedFrame))
        {
            ++m_selectedFrame;
            stopPlayback(document);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
    {
        data.frames.erase(data.frames.begin() + static_cast<std::ptrdiff_t>(m_selectedFrame));
        if (!data.frames.empty())
            m_selectedFrame = std::min(m_selectedFrame, data.frames.size() - 1);
        document.markDirty();
        stopPlayback(document);
        return;
    }
    ImGui::BeginDisabled(m_selectedFrame == 0);
    if (ImGui::Button("Move Left"))
    {
        std::swap(data.frames[m_selectedFrame], data.frames[m_selectedFrame - 1]);
        --m_selectedFrame;
        document.markDirty();
        stopPlayback(document);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_selectedFrame + 1 >= data.frames.size());
    if (ImGui::Button("Move Right"))
    {
        std::swap(data.frames[m_selectedFrame], data.frames[m_selectedFrame + 1]);
        ++m_selectedFrame;
        document.markDirty();
        stopPlayback(document);
    }
    ImGui::EndDisabled();
}

bool FrameAnimationEditor::acceptFrameDrop(AniDocument& document, std::size_t insertIndex, bool replace)
{
    if (!ImGui::BeginDragDropTarget())
        return false;
    bool accepted = false;
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayloadType);
        payload && payload->DataSize == sizeof(AssetDragPayload))
    {
        const auto& asset = *static_cast<const AssetDragPayload*>(payload->Data);
        if (isSupportedFrameAsset(asset))
        {
            auto& frames             = document.animation().frames;
            AnimationFrameData frame = frameFromPayload(asset);
            if (replace && insertIndex < frames.size())
            {
                frames[insertIndex].sourceType = frame.sourceType;
                frames[insertIndex].path       = std::move(frame.path);
                frames[insertIndex].atlasPath  = std::move(frame.atlasPath);
                frames[insertIndex].frameName  = std::move(frame.frameName);
            }
            else
            {
                insertIndex = std::min(insertIndex, frames.size());
                frames.insert(frames.begin() + static_cast<std::ptrdiff_t>(insertIndex), std::move(frame));
            }
            m_selectedFrame = std::min(insertIndex, frames.size() - 1);
            document.markDirty();
            stopPlayback(document);
            accepted = true;
        }
    }
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragBatchPayloadType);
        payload && payload->DataSize == sizeof(AssetDragPayloadBatch))
    {
        const auto& batch = *static_cast<const AssetDragPayloadBatch*>(payload->Data);
        auto& frames      = document.animation().frames;
        insertIndex       = std::min(insertIndex, frames.size());
        std::size_t added = 0;
        for (std::uint32_t index = 0; index < std::min<std::uint32_t>(batch.count, kMaxAssetDragBatchItems); ++index)
        {
            if (!isSupportedFrameAsset(batch.items[index]))
                continue;
            frames.insert(frames.begin() + static_cast<std::ptrdiff_t>(insertIndex + added),
                          frameFromPayload(batch.items[index]));
            ++added;
        }
        if (added > 0)
        {
            m_selectedFrame = insertIndex;
            document.markDirty();
            stopPlayback(document);
            accepted = true;
        }
    }
    ImGui::EndDragDropTarget();
    return accepted;
}
}  // namespace editor
