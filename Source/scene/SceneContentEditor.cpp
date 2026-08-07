#include "scene/SceneContentEditor.h"

#include "core/EditorContext.h"
#include "core/EditorPreferencesService.h"
#include "2d/RenderTexture.h"
#include "2d/Sprite.h"
#include "ImGui/ImGuiPresenter.h"
#include "imgui.h"
#include "renderer/Texture2D.h"
#include "scene/SceneNodeClipboardService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>

#ifndef AXMOL_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE
#    define AXMOL_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE 1
#endif

namespace editor
{
namespace
{
constexpr const char* kIconMousePointer = "\xef\x89\x85";  // fa-mouse-pointer
constexpr const char* kIconHandPaper = "\xef\x89\x96";     // fa-hand-paper
constexpr const char* kPanToolId = "canvas.pan";
constexpr float kSceneLeftToolbarWidth = 44.0f;
constexpr float kAlignmentToolbarButtonWidth = 34.0f;
constexpr float kAlignmentToolbarButtonHeight = 28.0f;
constexpr const char* kIconAlignLeft = "\xef\x80\xb6";     // fa-align-left
constexpr const char* kIconAlignCenter = "\xef\x80\xb7";   // fa-align-center
constexpr const char* kIconAlignRight = "\xef\x80\xb8";    // fa-align-right
constexpr const char* kIconArrowUp = "\xef\x81\xa2";       // fa-arrow-up
constexpr const char* kIconArrowsAltV = "\xef\x8c\xb8";    // fa-arrows-alt-v
constexpr const char* kIconArrowDown = "\xef\x81\xa3";     // fa-arrow-down

int countVisibleNodes(const SceneNode& node)
{
    if (!node.visible)
        return 0;

    int total = 1;
    for (const SceneNode& child : node.children)
        total += countVisibleNodes(child);
    return total;
}

ImFont* iconFontForGlyph(unsigned int glyph)
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    if (!atlas || atlas->Fonts.Size == 0)
        return nullptr;

    for (ImFont* font : atlas->Fonts)
    {
        if (font && font->IsGlyphInFont(static_cast<ImWchar>(glyph)) &&
            (font->IsGlyphInFont(0xf07b) || font->IsGlyphInFont(0xf06e)))
            return font;
    }
    return nullptr;
}

ImVec2 worldToScreen(const SceneCanvasFrame& frame, const ImVec2& world)
{
    return sceneWorldToScreen(frame, world);
}

ImVec2 screenToWorld(const SceneCanvasFrame& frame, const ImVec2& screen)
{
    return sceneScreenToWorld(frame, screen);
}

void drawCanvasGrid(ImDrawList* drawList, const SceneCanvasFrame& frame)
{
    if (drawList)
        drawSceneCanvasGrid(*drawList, frame);
}

struct RootBoundsAffine
{
    float a = 1.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
};

RootBoundsAffine multiplyAffine(const RootBoundsAffine& lhs, const RootBoundsAffine& rhs)
{
    return {lhs.a * rhs.a + lhs.c * rhs.b,
            lhs.b * rhs.a + lhs.d * rhs.b,
            lhs.a * rhs.c + lhs.c * rhs.d,
            lhs.b * rhs.c + lhs.d * rhs.d,
            lhs.a * rhs.tx + lhs.c * rhs.ty + lhs.tx,
            lhs.b * rhs.tx + lhs.d * rhs.ty + lhs.ty};
}

ImVec2 transformPoint(const RootBoundsAffine& transform, const ImVec2& point)
{
    return ImVec2(transform.a * point.x + transform.c * point.y + transform.tx,
                  transform.b * point.x + transform.d * point.y + transform.ty);
}

RootBoundsAffine rootBoundsTransform(const SceneNode& root)
{
    const float width = std::max(1.0f, root.size.width);
    const float height = std::max(1.0f, root.size.height);
    const float radians = -root.rotation * 3.14159265358979323846f / 180.0f;
    const float cosValue = std::cos(radians);
    const float sinValue = std::sin(radians);

    RootBoundsAffine translateToAnchor;
    translateToAnchor.tx = -root.anchor.x * width;
    translateToAnchor.ty = -root.anchor.y * height;

    RootBoundsAffine scale;
    scale.a = root.scale.x;
    scale.d = root.scale.y;

    RootBoundsAffine rotate;
    rotate.a = cosValue;
    rotate.b = sinValue;
    rotate.c = -sinValue;
    rotate.d = cosValue;

    RootBoundsAffine translateToPosition;
    translateToPosition.tx = root.position.x;
    translateToPosition.ty = root.position.y;

    return multiplyAffine(translateToPosition, multiplyAffine(rotate, multiplyAffine(scale, translateToAnchor)));
}

ImU32 sceneColorToU32(const SceneColor& color, float alphaScale = 1.0f)
{
    const auto component = [](float value) {
        return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return IM_COL32(component(color.r),
                    component(color.g),
                    component(color.b),
                    component(std::clamp(color.a * alphaScale, 0.0f, 1.0f)));
}

void drawRootBounds(ImDrawList& drawList, const SceneNode& root, const SceneCanvasFrame& frame, const SceneColor& color)
{
    const float width = std::max(1.0f, root.size.width);
    const float height = std::max(1.0f, root.size.height);
    const RootBoundsAffine transform = rootBoundsTransform(root);
    const std::array<ImVec2, 4> worldCorners = {transformPoint(transform, ImVec2(0.0f, 0.0f)),
                                                transformPoint(transform, ImVec2(width, 0.0f)),
                                                transformPoint(transform, ImVec2(width, height)),
                                                transformPoint(transform, ImVec2(0.0f, height))};

    ImVec2 screenCorners[4] = {};
    for (int index = 0; index < 4; ++index)
        screenCorners[index] = worldToScreen(frame, worldCorners[index]);

    drawList.AddConvexPolyFilled(screenCorners, 4, sceneColorToU32(color, 0.09f));
    drawList.AddPolyline(screenCorners, 4, sceneColorToU32(color), ImDrawFlags_Closed, 1.5f);

    for (const ImVec2& point : screenCorners)
        drawList.AddCircleFilled(point, 3.0f, sceneColorToU32(color, 1.1f), 10);
}

SceneVec2 selectedKeyboardMoveDelta()
{
    SceneVec2 delta;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        delta.x -= 1.0f;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        delta.x += 1.0f;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        delta.y += 1.0f;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        delta.y -= 1.0f;
    return delta;
}

bool isKeyboardMoveKeyDown()
{
    return ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_RightArrow) ||
           ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow);
}

bool moveSelectedNode(SceneDocument& document, const SceneVec2& delta)
{
    if (delta.x == 0.0f && delta.y == 0.0f)
        return false;

    SceneNode* node = document.selectedNode();
    if (!node || node->id == "root" || node->locked)
        return false;

    node->position.x += delta.x;
    node->position.y += delta.y;
    document.markDirty();
    return true;
}
}  // namespace

void SceneContentEditor::update(float deltaTime)
{
    m_previewRuntime.update(deltaTime);
}

void SceneContentEditor::reset()
{
    m_previewRuntime.reset();
    m_nodeSelectTool.reset();
    m_keyboardMoveTransactionActive = false;
}

void SceneContentEditor::draw(EditorContext& context,
                              SceneDocument& document,
                              const std::string& displayName,
                              const std::filesystem::path& documentKey,
                              const SceneContentExtensionHooks& extensionHooks)
{
    const SceneNode* selected = document.selectedNode();
    ImGui::Text("Scene: %s", displayName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Nodes: %d  Visible: %d  Zoom: %.0f%%  Selected: %s",
                        document.nodeCount(),
                        countVisibleNodes(document.root()),
                        m_zoom * 100.0f,
                        selected ? selected->name.c_str() : "None");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset View"))
    {
        m_zoom = 1.0f;
        m_pan = {};
    }
    ImGui::Separator();

    const ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const float contentHeight = std::max(availableSize.y, 240.0f);
    const float toolbarSpacing = ImGui::GetStyle().ItemSpacing.x;
    ImVec2 canvasSize(availableSize.x - kSceneLeftToolbarWidth - toolbarSpacing, contentHeight);
    canvasSize.x = std::max(canvasSize.x, 240.0f);

    drawLeftToolbar(context, document, extensionHooks, contentHeight);
    ImGui::SameLine();

    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 canvasEnd(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
    const ImVec2 baseCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
    SceneCanvasFrame frame{canvasPos, canvasEnd, canvasSize, baseCenter, m_pan, m_zoom};

    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
                         ImGui::IsMouseHoveringRect(canvasPos, canvasEnd, true);
    ImGuiIO& io = ImGui::GetIO();
    SceneCanvasViewState viewState{m_zoom, m_pan};
    const bool panToolActive = m_activeToolId == kPanToolId;
    const bool temporaryPanActive = hovered && ImGui::IsKeyDown(ImGuiKey_Space);
    const bool leftButtonPanning = (panToolActive || temporaryPanActive) &&
                                   ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f);
    handleSceneCanvasNavigation(io.MousePos,
                                io.MouseWheel,
                                io.MouseDelta,
                                hovered,
                                ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || leftButtonPanning,
                                viewState,
                                frame);
    m_zoom = viewState.zoom;
    m_pan = viewState.pan;

    SceneCanvasContext canvasContext{canvasPos,
                                     canvasEnd,
                                     canvasSize,
                                     hovered && !temporaryPanActive,
                                     m_zoom,
                                     std::string_view(m_activeToolId),
                                     ImGui::GetWindowDrawList(),
                                     [frame](const ImVec2& world) {
                                         return worldToScreen(frame, world);
                                     },
                                     [frame](const ImVec2& screen) {
                                         return screenToWorld(frame, screen);
                                     }};

    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::IsAnyItemActive())
    {
        bool handled = false;
        if (m_activeToolId != NodeSelectCanvasTool::kId && extensionHooks.deleteSelection)
            handled = extensionHooks.deleteSelection(document);
        if (!handled && m_activeToolId == NodeSelectCanvasTool::kId)
            document.deleteNode(document.selectedNodeId());
        m_nodeSelectTool.reset();
    }

    if (m_keyboardMoveTransactionActive && !isKeyboardMoveKeyDown())
        commitKeyboardMoveTransaction(document);

    const bool keyboardFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool blockedByOtherTransaction = document.hasActiveUndoTransaction() && !m_keyboardMoveTransactionActive;
    if (keyboardFocused && !ImGui::IsAnyItemActive() && !blockedByOtherTransaction)
    {
        handleNodeClipboardShortcuts(context, document, documentKey);

        const SceneVec2 delta = selectedKeyboardMoveDelta();
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            bool startedTransaction = false;
            if (!m_keyboardMoveTransactionActive)
            {
                m_keyboardMoveTransactionActive = document.beginUndoTransaction();
                startedTransaction = m_keyboardMoveTransactionActive;
            }

            bool handled = false;
            if (m_keyboardMoveTransactionActive)
            {
                if (extensionHooks.moveSelection)
                    handled = extensionHooks.moveSelection(document, delta);
                if (!handled)
                    handled = moveSelectedNode(document, delta);
            }

            if (handled)
                m_nodeSelectTool.reset();
            else if (startedTransaction)
            {
                document.cancelUndoTransaction();
                m_keyboardMoveTransactionActive = false;
            }
        }
    }

    if (m_activeToolId == NodeSelectCanvasTool::kId)
    {
        m_nodeSelectTool.handleInput(context, document, canvasContext);
    }
    else if (panToolActive)
    {
        m_nodeSelectTool.reset();
    }
    else
    {
        m_nodeSelectTool.reset();
        if (extensionHooks.handleCanvasInteraction)
            extensionHooks.handleCanvasInteraction(context, document, canvasContext);
    }

    m_previewRuntime.sync(document.root(), documentKey, frame, m_pan, m_zoom);
#if AXMOL_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE
    m_previewRuntime.renderToTexture(frame);
#endif

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvasPos, canvasEnd, true);
    drawList->AddRectFilled(canvasPos, canvasEnd, IM_COL32(28, 31, 36, 255));
    drawList->AddRect(canvasPos, canvasEnd, IM_COL32(72, 78, 86, 255));

    drawCanvasGrid(drawList, frame);
#if AXMOL_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE
    if (m_previewRuntime.renderTexture() && m_previewRuntime.renderTexture()->getSprite())
    {
        ax::Sprite* renderTextureSprite = m_previewRuntime.renderTexture()->getSprite();
        const float uMax =
            m_previewRuntime.renderTextureCapacityWidth() > 0
                ? std::clamp(static_cast<float>(m_previewRuntime.renderTextureRequestedWidth()) /
                                 static_cast<float>(m_previewRuntime.renderTextureCapacityWidth()),
                             0.0f,
                             1.0f)
                : 1.0f;
        const float vMax =
            m_previewRuntime.renderTextureCapacityHeight() > 0
                ? std::clamp(static_cast<float>(m_previewRuntime.renderTextureRequestedHeight()) /
                                 static_cast<float>(m_previewRuntime.renderTextureCapacityHeight()),
                             0.0f,
                             1.0f)
                : 1.0f;
        const ImVec2 uv0(0.0f, renderTextureSprite->isFlippedY() ? vMax : 0.0f);
        const ImVec2 uv1(uMax, renderTextureSprite->isFlippedY() ? 0.0f : vMax);
        ImGui::SetCursorScreenPos(canvasPos);
        ax::extension::ImGuiPresenter::getInstance()->image(renderTextureSprite->getTexture(), canvasSize, uv0, uv1);
    }
#else
    if (m_previewRuntime.rootNode())
    {
        ImGui::SetCursorScreenPos(canvasPos);
        ax::extension::ImGuiPresenter::getInstance()->node(m_previewRuntime.rootNode());
    }
#endif

    bool showRootBounds = true;
    SceneColor rootBoundsColor = {104.0f / 255.0f, 180.0f / 255.0f, 1.0f, 210.0f / 255.0f};
    if (auto preferences = context.services().get<EditorPreferencesService>())
    {
        showRootBounds = preferences->showRootBounds();
        rootBoundsColor = preferences->rootBoundsColor();
    }
    if (showRootBounds)
        drawRootBounds(*drawList, document.root(), frame, rootBoundsColor);

    if (extensionHooks.drawCanvasOverlay)
        extensionHooks.drawCanvasOverlay(context, document, canvasContext);
    if (m_activeToolId == NodeSelectCanvasTool::kId)
        m_nodeSelectTool.drawOverlay(context, document, canvasContext);

    if (hovered)
    {
        const ImVec2 mouseWorld = screenToWorld(frame, io.MousePos);
        char label[96] = {};
        std::snprintf(label, sizeof(label), "X %.1f  Y %.1f", mouseWorld.x, mouseWorld.y);
        const ImVec2 labelSize = ImGui::CalcTextSize(label);
        const ImVec2 labelMin(canvasEnd.x - labelSize.x - 12.0f, canvasEnd.y - labelSize.y - 10.0f);
        const ImVec2 labelMax(canvasEnd.x - 6.0f, canvasEnd.y - 5.0f);
        drawList->AddRectFilled(labelMin, labelMax, IM_COL32(18, 21, 25, 220), 3.0f);
        drawList->AddText(ImVec2(labelMin.x + 5.0f, labelMin.y + 3.0f), IM_COL32(214, 222, 230, 255), label);
    }

    drawList->PopClipRect();
}

void SceneContentEditor::drawLeftToolbar(EditorContext& context,
                                         SceneDocument& document,
                                         const SceneContentExtensionHooks& extensionHooks,
                                         float height)
{
    const std::string previousToolId = m_activeToolId;
    SceneCanvasToolbarContext toolbar(m_activeToolId);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 21, 25, 230));
    ImGui::BeginChild("SceneLeftToolbar", ImVec2(kSceneLeftToolbarWidth, height), true);

    toolbar.drawToolButton(NodeSelectCanvasTool::kId,
                           "Node Edit",
                           kIconMousePointer,
                           0xf245,
                           "Edit and drag nodes on the content canvas",
                           "N");
    toolbar.drawToolButton(kPanToolId,
                           "Pan",
                           kIconHandPaper,
                           0xf256,
                           "Drag the canvas with the left mouse button",
                           "P");

    if (extensionHooks.drawToolbar)
        extensionHooks.drawToolbar(context, document, toolbar);

    ImGui::Separator();
    drawAlignmentToolbar(document);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (m_activeToolId != previousToolId)
    {
        commitKeyboardMoveTransaction(document);
        m_nodeSelectTool.cancel(document);
    }
}

void SceneContentEditor::drawAlignmentToolbar(SceneDocument& document)
{
    const bool enabled = document.canAlignSelectedNodeToParent();

    struct Tool
    {
        const char* label;
        const char* icon;
        unsigned int iconGlyph;
        const char* tooltip;
        SceneNodeParentAlignment alignment;
    };

    constexpr Tool tools[] = {
        {"L", kIconAlignLeft, 0xf036, "Align left", SceneNodeParentAlignment::Left},
        {"HC", kIconAlignCenter, 0xf037, "Align horizontal center", SceneNodeParentAlignment::HorizontalCenter},
        {"R", kIconAlignRight, 0xf038, "Align right", SceneNodeParentAlignment::Right},
        {"T", kIconArrowUp, 0xf062, "Align top", SceneNodeParentAlignment::Top},
        {"VC", kIconArrowsAltV, 0xf338, "Align vertical center", SceneNodeParentAlignment::VerticalCenter},
        {"B", kIconArrowDown, 0xf063, "Align bottom", SceneNodeParentAlignment::Bottom},
    };

    ImGui::PushID("AlignmentToolbar");
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(43, 48, 55, 245));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(64, 74, 86, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(76, 137, 210, 255));

    for (int index = 0; index < static_cast<int>(std::size(tools)); ++index)
    {
        const Tool& tool = tools[index];
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth > kAlignmentToolbarButtonWidth)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - kAlignmentToolbarButtonWidth) * 0.5f);

        if (!enabled)
            ImGui::BeginDisabled();

        ImFont* iconFont = iconFontForGlyph(tool.iconGlyph);
        const char* visibleLabel = iconFont ? tool.icon : tool.label;
        if (iconFont)
            ImGui::PushFont(iconFont, 0.0f);
        const bool clicked = ImGui::Button(visibleLabel, ImVec2(kAlignmentToolbarButtonWidth, kAlignmentToolbarButtonHeight));
        if (iconFont)
            ImGui::PopFont();

        if (clicked)
        {
            if (document.alignSelectedNodeToParent(tool.alignment))
                m_nodeSelectTool.reset();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tool.tooltip);

        if (!enabled)
            ImGui::EndDisabled();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

void SceneContentEditor::commitKeyboardMoveTransaction(SceneDocument& document)
{
    if (!m_keyboardMoveTransactionActive)
        return;

    document.commitUndoTransaction();
    m_keyboardMoveTransactionActive = false;
}

void SceneContentEditor::handleNodeClipboardShortcuts(EditorContext& context,
                                                      SceneDocument& document,
                                                      const std::filesystem::path& documentKey)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl)
        return;

    auto clipboard = context.services().get<SceneNodeClipboardService>();
    if (!clipboard)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        clipboard->copySelectedNode(document, documentKey);
        return;
    }

    if (!clipboard->hasNode() || !ImGui::IsKeyPressed(ImGuiKey_V, false))
        return;

    if (m_keyboardMoveTransactionActive)
        commitKeyboardMoveTransaction(document);
    if (document.hasActiveUndoTransaction())
        return;

    if (clipboard->pasteNode(document, documentKey))
        m_nodeSelectTool.reset();
}
}  // namespace editor
