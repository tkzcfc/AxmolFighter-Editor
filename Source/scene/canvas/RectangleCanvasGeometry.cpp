#include "scene/canvas/RectangleCanvasGeometry.h"

#include <algorithm>
#include <cmath>

namespace editor
{
namespace
{
ImVec2 rotatePoint(const ImVec2& point, float radians)
{
    const float cosine = std::cos(radians);
    const float sine   = std::sin(radians);
    return ImVec2(cosine * point.x - sine * point.y, sine * point.x + cosine * point.y);
}
}  // namespace

ImVec2 rectangleLocalCorner(const SceneSize& size, int index)
{
    const float halfWidth  = std::max(1.0f, size.width) * 0.5f;
    const float halfHeight = std::max(1.0f, size.height) * 0.5f;
    switch (index)
    {
    case 1:
        return ImVec2(halfWidth, -halfHeight);
    case 2:
        return ImVec2(halfWidth, halfHeight);
    case 3:
        return ImVec2(-halfWidth, halfHeight);
    default:
        return ImVec2(-halfWidth, -halfHeight);
    }
}

bool pointInRectangle(const SceneVec2& position, const SceneSize& size, const ImVec2& point)
{
    const float halfWidth  = std::max(1.0f, size.width) * 0.5f;
    const float halfHeight = std::max(1.0f, size.height) * 0.5f;
    return point.x >= position.x - halfWidth && point.x <= position.x + halfWidth &&
           point.y >= position.y - halfHeight && point.y <= position.y + halfHeight;
}

void resizeRectangleFromCorner(SceneVec2& position,
                               SceneSize& size,
                               int draggedIndex,
                               const ImVec2& mouseWorld,
                               const ImVec2& fixedWorld,
                               float rotationDegrees)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float radians = rotationDegrees * kPi / 180.0f;
    const ImVec2 worldDelta(mouseWorld.x - fixedWorld.x, mouseWorld.y - fixedWorld.y);
    const ImVec2 localDelta = rotatePoint(worldDelta, -radians);

    size.width  = std::max(1.0f, std::abs(localDelta.x));
    size.height = std::max(1.0f, std::abs(localDelta.y));

    const int fixedIndex     = (draggedIndex + 2) % 4;
    const ImVec2 fixedLocal  = rectangleLocalCorner(size, fixedIndex);
    const ImVec2 fixedOffset = rotatePoint(fixedLocal, radians);
    position.x               = fixedWorld.x - fixedOffset.x;
    position.y               = fixedWorld.y - fixedOffset.y;
}
}  // namespace editor
