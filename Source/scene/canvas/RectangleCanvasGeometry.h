#pragma once

#include "scene/SceneTypes.h"
#include "imgui.h"

namespace editor
{
ImVec2 rectangleLocalCorner(const SceneSize& size, int index);
bool pointInRectangle(const SceneVec2& position, const SceneSize& size, const ImVec2& point);
void resizeRectangleFromCorner(SceneVec2& position,
                               SceneSize& size,
                               int draggedIndex,
                               const ImVec2& mouseWorld,
                               const ImVec2& fixedWorld,
                               float rotationDegrees = 0.0f);
}  // namespace editor
