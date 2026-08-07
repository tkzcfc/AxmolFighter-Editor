#include "scene/canvas/SceneCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace editor
{
ImVec2 sceneWorldToScreen(const SceneCanvasFrame& frame, const ImVec2& world)
{
    return ImVec2(frame.baseCenter.x + frame.pan.x + world.x * frame.zoom,
                  frame.baseCenter.y + frame.pan.y - world.y * frame.zoom);
}

ImVec2 sceneScreenToWorld(const SceneCanvasFrame& frame, const ImVec2& screen)
{
    return ImVec2((screen.x - frame.baseCenter.x - frame.pan.x) / frame.zoom,
                  -(screen.y - frame.baseCenter.y - frame.pan.y) / frame.zoom);
}

namespace
{
float chooseGridStep(float zoom)
{
    constexpr float targetScreenStep = 80.0f;
    constexpr float steps[] = {1.0f, 2.0f, 5.0f};
    float base = 1.0f;
    while (base * zoom < targetScreenStep)
        base *= 10.0f;

    for (float step : steps)
    {
        const float candidate = base * step;
        if (candidate * zoom >= targetScreenStep)
            return candidate;
    }
    return base * 10.0f;
}
}  // namespace

void drawSceneCanvasGrid(ImDrawList& drawList, const SceneCanvasFrame& frame)
{
    const ImVec2 worldMin = sceneScreenToWorld(frame, ImVec2(frame.canvasPos.x, frame.canvasEnd.y));
    const ImVec2 worldMax = sceneScreenToWorld(frame, ImVec2(frame.canvasEnd.x, frame.canvasPos.y));
    const float majorStep = chooseGridStep(frame.zoom);
    const float minorStep = majorStep / 4.0f;

    auto drawVerticalGrid = [&](float step, ImU32 color) {
        const float first = std::floor(worldMin.x / step) * step;
        for (float x = first; x <= worldMax.x; x += step)
        {
            const float sx = sceneWorldToScreen(frame, ImVec2(x, 0.0f)).x;
            drawList.AddLine(ImVec2(sx, frame.canvasPos.y), ImVec2(sx, frame.canvasEnd.y), color);
        }
    };

    auto drawHorizontalGrid = [&](float step, ImU32 color) {
        const float first = std::floor(worldMin.y / step) * step;
        for (float y = first; y <= worldMax.y; y += step)
        {
            const float sy = sceneWorldToScreen(frame, ImVec2(0.0f, y)).y;
            drawList.AddLine(ImVec2(frame.canvasPos.x, sy), ImVec2(frame.canvasEnd.x, sy), color);
        }
    };

    drawVerticalGrid(minorStep, IM_COL32(255, 255, 255, 14));
    drawHorizontalGrid(minorStep, IM_COL32(255, 255, 255, 14));
    drawVerticalGrid(majorStep, IM_COL32(255, 255, 255, 32));
    drawHorizontalGrid(majorStep, IM_COL32(255, 255, 255, 32));

    const ImVec2 origin = sceneWorldToScreen(frame, ImVec2(0.0f, 0.0f));
    if (origin.y >= frame.canvasPos.y && origin.y <= frame.canvasEnd.y)
        drawList.AddLine(ImVec2(frame.canvasPos.x, origin.y), ImVec2(frame.canvasEnd.x, origin.y),
                         IM_COL32(255, 255, 255, 72));
    if (origin.x >= frame.canvasPos.x && origin.x <= frame.canvasEnd.x)
        drawList.AddLine(ImVec2(origin.x, frame.canvasPos.y), ImVec2(origin.x, frame.canvasEnd.y),
                         IM_COL32(255, 255, 255, 72));

    const float labelInset = 4.0f;
    const float rulerTop = frame.canvasPos.y + labelInset;
    const float rulerLeft = frame.canvasPos.x + labelInset;
    const float firstX = std::floor(worldMin.x / majorStep) * majorStep;
    for (float x = firstX; x <= worldMax.x; x += majorStep)
    {
        const float sx = sceneWorldToScreen(frame, ImVec2(x, 0.0f)).x;
        if (sx < frame.canvasPos.x + 24.0f || sx > frame.canvasEnd.x - 24.0f)
            continue;

        char label[32] = {};
        std::snprintf(label, sizeof(label), "%.0f", x);
        drawList.AddText(ImVec2(sx + 3.0f, rulerTop), IM_COL32(172, 181, 190, 220), label);
        drawList.AddLine(ImVec2(sx, frame.canvasPos.y), ImVec2(sx, frame.canvasPos.y + 8.0f),
                         IM_COL32(172, 181, 190, 180));
    }

    const float firstY = std::floor(worldMin.y / majorStep) * majorStep;
    for (float y = firstY; y <= worldMax.y; y += majorStep)
    {
        const float sy = sceneWorldToScreen(frame, ImVec2(0.0f, y)).y;
        if (sy < frame.canvasPos.y + 18.0f || sy > frame.canvasEnd.y - 18.0f)
            continue;

        char label[32] = {};
        std::snprintf(label, sizeof(label), "%.0f", y);
        drawList.AddText(ImVec2(rulerLeft, sy + 3.0f), IM_COL32(172, 181, 190, 220), label);
        drawList.AddLine(ImVec2(frame.canvasPos.x, sy), ImVec2(frame.canvasPos.x + 8.0f, sy),
                         IM_COL32(172, 181, 190, 180));
    }
}

void handleSceneCanvasNavigation(const ImVec2& mousePosition,
                                 float mouseWheel,
                                 const ImVec2& mouseDelta,
                                 bool hovered,
                                 bool middleDragging,
                                 SceneCanvasViewState& viewState,
                                 SceneCanvasFrame& frame)
{
    if (hovered && mouseWheel != 0.0f)
    {
        const ImVec2 mouseWorld = sceneScreenToWorld(frame, mousePosition);
        const float zoomFactor = mouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f;
        viewState.zoom = std::clamp(viewState.zoom * zoomFactor, kMinSceneCanvasZoom, kMaxSceneCanvasZoom);
        viewState.pan.x = mousePosition.x - frame.baseCenter.x - mouseWorld.x * viewState.zoom;
        viewState.pan.y = mousePosition.y - frame.baseCenter.y + mouseWorld.y * viewState.zoom;
        frame.pan = viewState.pan;
        frame.zoom = viewState.zoom;
    }

    if (hovered && middleDragging)
    {
        viewState.pan.x += mouseDelta.x;
        viewState.pan.y += mouseDelta.y;
        frame.pan = viewState.pan;
    }
}
}  // namespace editor
