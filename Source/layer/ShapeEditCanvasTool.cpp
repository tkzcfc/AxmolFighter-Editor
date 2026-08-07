#include "layer/ShapeEditCanvasTool.h"

#include "core/EditorContext.h"
#include "imgui.h"
#include "scene/canvas/RectangleCanvasGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace editor
{
namespace
{
constexpr float kHandleRadius = 7.0f;

struct ShapeAffine
{
    float a  = 1.0f;
    float b  = 0.0f;
    float c  = 0.0f;
    float d  = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
};

ImVec2 transformPoint(const ShapeAffine& transform, const ImVec2& point)
{
    return ImVec2(transform.a * point.x + transform.c * point.y + transform.tx,
                  transform.b * point.x + transform.d * point.y + transform.ty);
}

ImVec2 inverseTransformPoint(const ShapeAffine& transform, const ImVec2& point)
{
    const float determinant = transform.a * transform.d - transform.b * transform.c;
    if (std::abs(determinant) <= 0.000001f)
        return point;

    const float inverseDeterminant = 1.0f / determinant;
    const float x                  = point.x - transform.tx;
    const float y                  = point.y - transform.ty;
    return ImVec2((transform.d * x - transform.c * y) * inverseDeterminant,
                  (-transform.b * x + transform.a * y) * inverseDeterminant);
}

ShapeAffine shapeTransform(const LayerShape& shape)
{
    const float radians  = shape.rotation * 3.14159265358979323846f / 180.0f;
    const float cosValue = std::cos(radians);
    const float sinValue = std::sin(radians);
    return {cosValue, sinValue, -sinValue, cosValue, shape.position.x, shape.position.y};
}

ImU32 shapeColorU32(const LayerShape& shape, int alphaOverride = -1)
{
    const int alpha = alphaOverride >= 0 ? alphaOverride : std::clamp(shape.opacity, 0, 255);
    return IM_COL32(static_cast<int>(std::clamp(shape.color.r, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(shape.color.g, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(shape.color.b, 0.0f, 1.0f) * 255.0f), alpha);
}

ImU32 shapeStrokeColorU32(const LayerShape& shape, int alphaOverride = -1)
{
    const int alpha = alphaOverride >= 0 ? alphaOverride : std::clamp(shape.opacity, 0, 255);
    return IM_COL32(static_cast<int>(std::clamp(shape.strokeColor.r, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(shape.strokeColor.g, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(shape.strokeColor.b, 0.0f, 1.0f) * 255.0f), alpha);
}

void drawShapeNameLabel(ImDrawList& drawList, const LayerShape& shape, const std::vector<ImVec2>& screenPoints)
{
    if (shape.name.empty() || screenPoints.empty())
        return;

    ImVec2 min = screenPoints.front();
    ImVec2 max = screenPoints.front();
    for (const ImVec2& point : screenPoints)
    {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
    }

    const ImVec2 textSize = ImGui::CalcTextSize(shape.name.c_str());
    const ImVec2 padding(10.0f, 6.0f);
    const float width  = textSize.x + padding.x * 2.0f;
    const float height = textSize.y + padding.y * 2.0f;
    const ImVec2 center((min.x + max.x) * 0.5f, min.y - height * 0.5f - 8.0f);
    const ImVec2 rectMin(center.x - width * 0.5f, center.y - height * 0.5f);
    const ImVec2 rectMax(center.x + width * 0.5f, center.y + height * 0.5f);
    const ImVec2 textPos(rectMin.x + padding.x, rectMin.y + padding.y);

    drawList.AddRectFilled(rectMin, rectMax, IM_COL32(54, 56, 60, 232), 6.0f);
    drawList.AddText(textPos, IM_COL32(245, 247, 250, 255), shape.name.c_str());
}

std::vector<ImVec2> shapeWorldPoints(const LayerShape& shape)
{
    const ShapeAffine transform = shapeTransform(shape);
    std::vector<ImVec2> points;
    if (shape.type == "Circle")
    {
        const int segments = std::clamp(shape.segments, 3, 256);
        points.reserve(segments);
        const float radius = std::max(1.0f, shape.radius);
        for (int index = 0; index < segments; ++index)
        {
            const float angle =
                (static_cast<float>(index) / static_cast<float>(segments)) * 2.0f * 3.14159265358979323846f;
            points.push_back(transformPoint(transform, ImVec2(std::cos(angle) * radius, std::sin(angle) * radius)));
        }
    }
    else if (shape.type == "Polygon")
    {
        points.reserve(shape.points.size());
        for (const SceneVec2& point : shape.points)
            points.push_back(transformPoint(transform, ImVec2(point.x, point.y)));
    }
    else
    {
        points.reserve(4);
        for (int index = 0; index < 4; ++index)
            points.push_back(transformPoint(transform, rectangleLocalCorner(shape.size, index)));
    }
    return points;
}

bool pointInPolygon(const std::vector<ImVec2>& polygon, const ImVec2& point)
{
    if (polygon.size() < 3)
        return false;

    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        const ImVec2& a       = polygon[i];
        const ImVec2& b       = polygon[j];
        const bool intersects = ((a.y > point.y) != (b.y > point.y)) &&
                                (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 0.000001f) + a.x);
        if (intersects)
            inside = !inside;
    }
    return inside;
}

LayerShape* findShape(LayerDocument& document, const std::string& id)
{
    for (LayerShape& shape : document.shapes())
    {
        if (shape.id == id)
            return &shape;
    }
    return nullptr;
}

const LayerShape* hitTestShape(const LayerDocument& document, const ImVec2& world)
{
    const std::vector<LayerShape>& shapes = document.shapes();
    for (auto it = shapes.rbegin(); it != shapes.rend(); ++it)
    {
        if (!it->visible || it->locked)
            continue;
        if (pointInPolygon(shapeWorldPoints(*it), world))
            return &*it;
    }
    return nullptr;
}

void drawControlPoint(ImDrawList& drawList, const ImVec2& screenPosition, bool enabled)
{
    const ImU32 fill = enabled ? IM_COL32(255, 204, 64, 255) : IM_COL32(255, 112, 112, 220);
    drawList.AddCircleFilled(screenPosition, 4.5f, fill, 12);
    drawList.AddCircle(screenPosition, 6.5f, IM_COL32(18, 21, 25, 240), 12, 1.5f);
}

bool hitControlPoint(const SceneCanvasContext& canvas, const ImVec2& worldPosition)
{
    const ImVec2 screenPosition = canvas.worldToScreen(worldPosition);
    const ImVec2 mousePosition  = ImGui::GetIO().MousePos;
    const float dx              = mousePosition.x - screenPosition.x;
    const float dy              = mousePosition.y - screenPosition.y;
    return dx * dx + dy * dy <= kHandleRadius * kHandleRadius;
}

int hitShapeHandle(const LayerShape& shape, const SceneCanvasContext& canvas)
{
    if (shape.locked || !shape.visible)
        return -1;

    const ShapeAffine transform = shapeTransform(shape);
    if (shape.type == "Circle")
    {
        if (hitControlPoint(canvas, transformPoint(transform, ImVec2(std::max(1.0f, shape.radius), 0.0f))))
            return 0;
        return -1;
    }

    if (shape.type == "Polygon")
    {
        for (int index = static_cast<int>(shape.points.size()) - 1; index >= 0; --index)
        {
            const SceneVec2& point = shape.points[index];
            if (hitControlPoint(canvas, transformPoint(transform, ImVec2(point.x, point.y))))
                return index;
        }
        return -1;
    }

    for (int index = 3; index >= 0; --index)
    {
        if (hitControlPoint(canvas, transformPoint(transform, rectangleLocalCorner(shape.size, index))))
            return index;
    }
    return -1;
}

}  // namespace

std::string_view ShapeEditCanvasTool::id() const
{
    return kId;
}

std::string_view ShapeEditCanvasTool::displayName() const
{
    return "Shape Edit";
}

bool ShapeEditCanvasTool::handleInput(EditorContext&, LayerDocument& document, const SceneCanvasContext& canvas) const
{
    if (canvas.activeToolId != kId)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    if ((!m_draggingShapeId.empty() || !m_resizingShapeId.empty()) && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        document.commitUndoTransaction();
        reset();
        return true;
    }

    const bool middleDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    if (!canvas.hovered || middleDragging)
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            reset();
        return true;
    }

    const ImVec2 mouseWorld = canvas.screenToWorld(io.MousePos);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        reset();

        if (LayerShape* selected = document.selectedShape())
        {
            const int handleIndex = hitShapeHandle(*selected, canvas);
            if (handleIndex >= 0)
            {
                document.beginUndoTransaction();
                m_resizingShapeId   = selected->id;
                m_resizeHandleIndex = handleIndex;
                if (selected->type == "Rectangle")
                {
                    const int fixedIndex = (handleIndex + 2) % 4;
                    const ImVec2 fixedWorld =
                        transformPoint(shapeTransform(*selected), rectangleLocalCorner(selected->size, fixedIndex));
                    m_resizeFixedWorld = {fixedWorld.x, fixedWorld.y};
                }
                return true;
            }
        }

        if (const LayerShape* hit = hitTestShape(document, mouseWorld))
        {
            document.selectShape(hit->id);
            if (!hit->locked)
            {
                document.beginUndoTransaction();
                m_draggingShapeId        = hit->id;
                m_dragStartWorld         = {mouseWorld.x, mouseWorld.y};
                m_dragStartShapePosition = {hit->position.x, hit->position.y};
            }
            return true;
        }

        document.clearShapeSelection();
        return true;
    }

    if (!m_resizingShapeId.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        LayerShape* shape = findShape(document, m_resizingShapeId);
        if (!shape || shape->locked)
        {
            document.commitUndoTransaction();
            reset();
            return true;
        }

        if (shape->type == "Circle")
        {
            const ImVec2 local = inverseTransformPoint(shapeTransform(*shape), mouseWorld);
            shape->radius      = std::max(1.0f, std::sqrt(local.x * local.x + local.y * local.y));
        }
        else if (shape->type == "Polygon")
        {
            if (m_resizeHandleIndex >= 0 && m_resizeHandleIndex < static_cast<int>(shape->points.size()))
            {
                const ImVec2 local                 = inverseTransformPoint(shapeTransform(*shape), mouseWorld);
                shape->points[m_resizeHandleIndex] = {local.x, local.y};
            }
        }
        else
        {
            resizeRectangleFromCorner(shape->position, shape->size, m_resizeHandleIndex, mouseWorld,
                                      ImVec2(m_resizeFixedWorld.x, m_resizeFixedWorld.y), shape->rotation);
        }
        document.markDirty();
        return true;
    }

    if (!m_draggingShapeId.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        LayerShape* shape = findShape(document, m_draggingShapeId);
        if (!shape || shape->locked)
        {
            document.commitUndoTransaction();
            m_draggingShapeId.clear();
            return true;
        }

        shape->position.x = m_dragStartShapePosition.x + (mouseWorld.x - m_dragStartWorld.x);
        shape->position.y = m_dragStartShapePosition.y + (mouseWorld.y - m_dragStartWorld.y);
        document.markDirty();
        return true;
    }

    return true;
}

void ShapeEditCanvasTool::drawOverlay(EditorContext&, LayerDocument& document, const SceneCanvasContext& canvas) const
{
    if (!canvas.drawList)
        return;

    for (const LayerShape& shape : document.shapes())
    {
        if (!shape.visible)
            continue;

        std::vector<ImVec2> screenPoints;
        const std::vector<ImVec2> worldPoints = shapeWorldPoints(shape);
        screenPoints.reserve(worldPoints.size());
        for (const ImVec2& point : worldPoints)
            screenPoints.push_back(canvas.worldToScreen(point));

        if (screenPoints.size() < 3)
            continue;

        const ImU32 fill    = shapeColorU32(shape, std::clamp(shape.opacity / 2, 24, 160));
        const ImU32 outline = document.selectedShapeId() == shape.id
                                  ? IM_COL32(255, 204, 64, 255)
                                  : shapeStrokeColorU32(shape, std::clamp(shape.opacity, 64, 255));
        canvas.drawList->AddConvexPolyFilled(screenPoints.data(), static_cast<int>(screenPoints.size()), fill);
        canvas.drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()), outline,
                                     ImDrawFlags_Closed, 2.0f);
        drawShapeNameLabel(*canvas.drawList, shape, screenPoints);
    }

    if (canvas.activeToolId != kId)
        return;

    const LayerShape* selected = document.selectedShape();
    if (!selected || !selected->visible)
        return;

    const bool enabled          = !selected->locked;
    const ShapeAffine transform = shapeTransform(*selected);
    if (selected->type == "Circle")
    {
        drawControlPoint(
            *canvas.drawList,
            canvas.worldToScreen(transformPoint(transform, ImVec2(std::max(1.0f, selected->radius), 0.0f))), enabled);
    }
    else if (selected->type == "Polygon")
    {
        for (const SceneVec2& point : selected->points)
            drawControlPoint(*canvas.drawList,
                             canvas.worldToScreen(transformPoint(transform, ImVec2(point.x, point.y))), enabled);
    }
    else
    {
        for (int index = 0; index < 4; ++index)
            drawControlPoint(
                *canvas.drawList,
                canvas.worldToScreen(transformPoint(transform, rectangleLocalCorner(selected->size, index))), enabled);
    }
}

void ShapeEditCanvasTool::cancel(LayerDocument& document) const
{
    if (!m_draggingShapeId.empty() || !m_resizingShapeId.empty())
        document.commitUndoTransaction();
    reset();
}

void ShapeEditCanvasTool::reset() const
{
    m_draggingShapeId.clear();
    m_resizingShapeId.clear();
    m_resizeHandleIndex = -1;
}
}  // namespace editor
