#pragma once

#include "imgui.h"

#include <functional>
#include <string_view>

namespace editor
{
struct SceneCanvasFrame
{
    ImVec2 canvasPos;
    ImVec2 canvasEnd;
    ImVec2 canvasSize;
    ImVec2 baseCenter;
    ImVec2 pan;
    float zoom = 1.0f;
};

struct SceneCanvasViewState
{
    float zoom = 1.0f;
    ImVec2 pan;
};

struct SceneCanvasContext
{
    ImVec2 canvasPos;
    ImVec2 canvasEnd;
    ImVec2 canvasSize;
    bool hovered = false;
    float zoom = 1.0f;
    std::string_view activeToolId;
    ImDrawList* drawList = nullptr;
    std::function<ImVec2(const ImVec2&)> worldToScreen;
    std::function<ImVec2(const ImVec2&)> screenToWorld;
};

constexpr float kMinSceneCanvasZoom = 0.1f;
constexpr float kMaxSceneCanvasZoom = 8.0f;

ImVec2 sceneWorldToScreen(const SceneCanvasFrame& frame, const ImVec2& world);
ImVec2 sceneScreenToWorld(const SceneCanvasFrame& frame, const ImVec2& screen);
void drawSceneCanvasGrid(ImDrawList& drawList, const SceneCanvasFrame& frame);
void handleSceneCanvasNavigation(const ImVec2& mousePosition,
                                 float mouseWheel,
                                 const ImVec2& mouseDelta,
                                 bool hovered,
                                 bool middleDragging,
                                 SceneCanvasViewState& viewState,
                                 SceneCanvasFrame& frame);

}  // namespace editor
