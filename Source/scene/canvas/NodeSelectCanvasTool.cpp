#include "scene/canvas/NodeSelectCanvasTool.h"

#include "scene/SceneDocument.h"
#include "scene/node_editors/SceneNodeEditor.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace editor
{
namespace
{
constexpr float kControlPointRadius = 7.0f;

struct SceneNodeBounds
{
    ImVec2 min;
    ImVec2 max;
};

struct SceneAffine
{
    float a = 1.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
};

SceneAffine multiplyAffine(const SceneAffine& lhs, const SceneAffine& rhs)
{
    return {lhs.a * rhs.a + lhs.c * rhs.b,
            lhs.b * rhs.a + lhs.d * rhs.b,
            lhs.a * rhs.c + lhs.c * rhs.d,
            lhs.b * rhs.c + lhs.d * rhs.d,
            lhs.a * rhs.tx + lhs.c * rhs.ty + lhs.tx,
            lhs.b * rhs.tx + lhs.d * rhs.ty + lhs.ty};
}

ImVec2 transformPoint(const SceneAffine& transform, const ImVec2& point)
{
    return ImVec2(transform.a * point.x + transform.c * point.y + transform.tx,
                  transform.b * point.x + transform.d * point.y + transform.ty);
}

ImVec2 inverseTransformPoint(const SceneAffine& transform, const ImVec2& point)
{
    const float determinant = transform.a * transform.d - transform.b * transform.c;
    if (std::abs(determinant) <= 0.000001f)
        return point;

    const float inverseDeterminant = 1.0f / determinant;
    const float x = point.x - transform.tx;
    const float y = point.y - transform.ty;
    return ImVec2((transform.d * x - transform.c * y) * inverseDeterminant,
                  (-transform.b * x + transform.a * y) * inverseDeterminant);
}

SceneAffine nodeLocalTransform(const SceneNode& node)
{
    if (node.id == "root")
        return {};

    const float width = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);
    const float radians = -node.rotation * 3.14159265358979323846f / 180.0f;
    const float cosValue = std::cos(radians);
    const float sinValue = std::sin(radians);
    const float anchorX = node.anchor.x * width;
    const float anchorY = node.anchor.y * height;

    SceneAffine translateToAnchor;
    translateToAnchor.tx = -anchorX;
    translateToAnchor.ty = -anchorY;

    SceneAffine scale;
    scale.a = node.scale.x;
    scale.d = node.scale.y;

    SceneAffine rotate;
    rotate.a = cosValue;
    rotate.b = sinValue;
    rotate.c = -sinValue;
    rotate.d = cosValue;

    SceneAffine translateToPosition;
    translateToPosition.tx = node.position.x;
    translateToPosition.ty = node.position.y;

    return multiplyAffine(translateToPosition, multiplyAffine(rotate, multiplyAffine(scale, translateToAnchor)));
}

SceneNodeBounds boundsFromCorners(const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d)
{
    return {ImVec2(std::min({a.x, b.x, c.x, d.x}), std::min({a.y, b.y, c.y, d.y})),
            ImVec2(std::max({a.x, b.x, c.x, d.x}), std::max({a.y, b.y, c.y, d.y}))};
}

SceneNodeBounds sceneNodeBounds(const SceneNode& node, const SceneAffine& nodeTransform)
{
    const float width = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);
    return boundsFromCorners(transformPoint(nodeTransform, ImVec2(0.0f, 0.0f)),
                             transformPoint(nodeTransform, ImVec2(width, 0.0f)),
                             transformPoint(nodeTransform, ImVec2(width, height)),
                             transformPoint(nodeTransform, ImVec2(0.0f, height)));
}

ImVec2 nodeLocalCorner(const SceneSize& size, int index)
{
    const float width = std::max(1.0f, size.width);
    const float height = std::max(1.0f, size.height);
    switch (index)
    {
    case 1:
        return ImVec2(width, 0.0f);
    case 2:
        return ImVec2(width, height);
    case 3:
        return ImVec2(0.0f, height);
    default:
        return ImVec2(0.0f, 0.0f);
    }
}

std::array<ImVec2, 4> sceneNodeWorldCorners(const SceneNode& node, const SceneAffine& nodeTransform)
{
    return {transformPoint(nodeTransform, nodeLocalCorner(node.size, 0)),
            transformPoint(nodeTransform, nodeLocalCorner(node.size, 1)),
            transformPoint(nodeTransform, nodeLocalCorner(node.size, 2)),
            transformPoint(nodeTransform, nodeLocalCorner(node.size, 3))};
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
    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    const float dx = mousePosition.x - screenPosition.x;
    const float dy = mousePosition.y - screenPosition.y;
    return dx * dx + dy * dy <= kControlPointRadius * kControlPointRadius;
}

bool containsWorldPoint(const SceneNodeBounds& bounds, const ImVec2& world)
{
    return world.x >= bounds.min.x && world.x <= bounds.max.x && world.y >= bounds.min.y && world.y <= bounds.max.y;
}

const SceneNode* hitTestSceneNode(const SceneNode& node, const SceneAffine& parentTransform, const ImVec2& world)
{
    if (!node.visible)
        return nullptr;

    const SceneAffine nodeTransform = multiplyAffine(parentTransform, nodeLocalTransform(node));
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it)
    {
        if (const SceneNode* hit = hitTestSceneNode(*it, nodeTransform, world))
            return hit;
    }

    if (node.id != "root" && !node.locked && containsWorldPoint(sceneNodeBounds(node, nodeTransform), world))
        return &node;
    return nullptr;
}

SceneNode* findSceneNodeMutable(SceneNode& node, const std::string& id)
{
    if (node.id == id)
        return &node;
    for (SceneNode& child : node.children)
    {
        if (SceneNode* result = findSceneNodeMutable(child, id))
            return result;
    }
    return nullptr;
}

const SceneNode* findSceneNodeWithTransform(const SceneNode& node,
                                            const std::string& id,
                                            const SceneAffine& parentTransform,
                                            SceneAffine& outNodeTransform)
{
    const SceneAffine nodeTransform = multiplyAffine(parentTransform, nodeLocalTransform(node));
    if (node.id == id)
    {
        outNodeTransform = nodeTransform;
        return &node;
    }

    for (const SceneNode& child : node.children)
    {
        if (const SceneNode* result = findSceneNodeWithTransform(child, id, nodeTransform, outNodeTransform))
            return result;
    }
    return nullptr;
}

const SceneNode* findSceneNodeWithParentTransform(const SceneNode& node,
                                                  const std::string& id,
                                                  const SceneAffine& parentTransform,
                                                  SceneAffine& outParentTransform)
{
    if (node.id == id)
    {
        outParentTransform = parentTransform;
        return &node;
    }

    const SceneAffine nodeTransform = multiplyAffine(parentTransform, nodeLocalTransform(node));
    for (const SceneNode& child : node.children)
    {
        if (const SceneNode* result = findSceneNodeWithParentTransform(child, id, nodeTransform, outParentTransform))
            return result;
    }
    return nullptr;
}

SceneNodeSizeEditPolicy sizeEditPolicyForNode(const SceneNode& node)
{
    if (const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(node.type))
        return editor->sizeEditPolicy(node);
    return {};
}

bool applyAfterCanvasResize(SceneNode& node)
{
    if (const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(node.type))
        return editor->afterCanvasResize(node);
    return false;
}

int hitNodeControlPoint(const SceneDocument& document, const SceneCanvasContext& canvas)
{
    SceneAffine selectedTransform;
    const SceneNode* selected = findSceneNodeWithTransform(document.root(), document.selectedNodeId(), {}, selectedTransform);
    if (!selected || selected->id == "root" || !selected->visible || selected->locked)
        return -1;

    const SceneNodeSizeEditPolicy policy = sizeEditPolicyForNode(*selected);
    if (!policy.widthEditable && !policy.heightEditable)
        return -1;

    const std::array<ImVec2, 4> corners = sceneNodeWorldCorners(*selected, selectedTransform);
    for (int index = 3; index >= 0; --index)
    {
        if (hitControlPoint(canvas, corners[index]))
            return index;
    }
    return -1;
}

void resizeSceneNodeFromCorner(SceneNode& node,
                               int draggedIndex,
                               const ImVec2& mouseParent,
                               const ImVec2& fixedParent,
                               const SceneNodeSizeEditPolicy& policy)
{
    const int fixedIndex = (draggedIndex + 2) % 4;
    const float radians = -node.rotation * 3.14159265358979323846f / 180.0f;
    const float cosValue = std::cos(radians);
    const float sinValue = std::sin(radians);

    SceneAffine linear;
    linear.a = cosValue * node.scale.x;
    linear.b = sinValue * node.scale.x;
    linear.c = -sinValue * node.scale.y;
    linear.d = cosValue * node.scale.y;

    const ImVec2 parentDelta(mouseParent.x - fixedParent.x, mouseParent.y - fixedParent.y);
    const ImVec2 localDelta = inverseTransformPoint(linear, parentDelta);

    const float draggedX = (draggedIndex == 1 || draggedIndex == 2) ? 1.0f : 0.0f;
    const float draggedY = (draggedIndex == 2 || draggedIndex == 3) ? 1.0f : 0.0f;
    const float fixedX = (fixedIndex == 1 || fixedIndex == 2) ? 1.0f : 0.0f;
    const float fixedY = (fixedIndex == 2 || fixedIndex == 3) ? 1.0f : 0.0f;

    if (policy.widthEditable)
        node.size.width = std::max(1.0f, (draggedX - fixedX) * localDelta.x);
    if (policy.heightEditable)
        node.size.height = std::max(1.0f, (draggedY - fixedY) * localDelta.y);

    const ImVec2 fixedLocal = nodeLocalCorner(node.size, fixedIndex);
    const ImVec2 anchorOffset(node.anchor.x * node.size.width, node.anchor.y * node.size.height);
    const ImVec2 fixedOffset = transformPoint(linear, ImVec2(fixedLocal.x - anchorOffset.x, fixedLocal.y - anchorOffset.y));
    node.position.x = fixedParent.x - fixedOffset.x;
    node.position.y = fixedParent.y - fixedOffset.y;
}
}  // namespace

std::string_view NodeSelectCanvasTool::id() const
{
    return kId;
}

std::string_view NodeSelectCanvasTool::displayName() const
{
    return "Node Edit";
}

bool NodeSelectCanvasTool::handleInput(EditorContext&, SceneDocument& document, const SceneCanvasContext& canvas) const
{
    ImGuiIO& io = ImGui::GetIO();
    if (!m_draggingNodeId.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        document.commitUndoTransaction();
        m_draggingNodeId.clear();
        return true;
    }
    if (!m_resizingNodeId.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        document.commitUndoTransaction();
        m_resizingNodeId.clear();
        m_resizeNodeHandleIndex = -1;
        return true;
    }

    const bool middleDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);
    if (!canvas.hovered || middleDragging)
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            reset();
        return false;
    }

    const ImVec2 mouseWorld = canvas.screenToWorld(io.MousePos);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const int resizeHandle = hitNodeControlPoint(document, canvas);
        if (resizeHandle >= 0)
        {
            SceneNode* selectedNode = document.selectedNode();
            if (selectedNode && selectedNode->id != "root" && !selectedNode->locked)
            {
                const SceneNodeSizeEditPolicy policy = sizeEditPolicyForNode(*selectedNode);
                if (!policy.widthEditable && !policy.heightEditable)
                    return false;

                const SceneAffine localTransform = nodeLocalTransform(*selectedNode);
                const int fixedIndex = (resizeHandle + 2) % 4;
                const ImVec2 fixedParent = transformPoint(localTransform, nodeLocalCorner(selectedNode->size, fixedIndex));
                document.beginUndoTransaction();
                m_draggingNodeId.clear();
                m_resizingNodeId = selectedNode->id;
                m_resizeNodeHandleIndex = resizeHandle;
                m_resizeFixedParent = {fixedParent.x, fixedParent.y};
                return true;
            }
        }

        const SceneNode* hit = hitTestSceneNode(document.root(), {}, mouseWorld);
        document.selectNode(hit ? hit->id : "root");
        reset();

        SceneNode* selectedNode = document.selectedNode();
        if (selectedNode && selectedNode->id != "root" && !selectedNode->locked)
        {
            SceneAffine parentTransform;
            findSceneNodeWithParentTransform(document.root(), selectedNode->id, {}, parentTransform);
            const ImVec2 mouseParentLocal = inverseTransformPoint(parentTransform, mouseWorld);
            document.beginUndoTransaction();
            m_draggingNodeId = selectedNode->id;
            m_dragStartWorld = {mouseParentLocal.x, mouseParentLocal.y};
            m_dragStartNodePosition = {selectedNode->position.x, selectedNode->position.y};
        }
        return true;
    }

    if (!m_resizingNodeId.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        SceneNode* node = findSceneNodeMutable(document.root(), m_resizingNodeId);
        if (!node || node->locked)
        {
            document.commitUndoTransaction();
            reset();
            return true;
        }

        const SceneNodeSizeEditPolicy policy = sizeEditPolicyForNode(*node);
        if (!policy.widthEditable && !policy.heightEditable)
        {
            document.commitUndoTransaction();
            reset();
            return true;
        }

        SceneAffine parentTransform;
        findSceneNodeWithParentTransform(document.root(), node->id, {}, parentTransform);
        const ImVec2 mouseParentLocal = inverseTransformPoint(parentTransform, mouseWorld);
        resizeSceneNodeFromCorner(*node,
                                  m_resizeNodeHandleIndex,
                                  mouseParentLocal,
                                  ImVec2(m_resizeFixedParent.x, m_resizeFixedParent.y),
                                  policy);
        applyAfterCanvasResize(*node);
        document.markDirty();
        return true;
    }

    if (!m_draggingNodeId.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        SceneNode* node = findSceneNodeMutable(document.root(), m_draggingNodeId);
        if (!node || node->locked)
        {
            document.commitUndoTransaction();
            m_draggingNodeId.clear();
            return true;
        }

        SceneAffine parentTransform;
        findSceneNodeWithParentTransform(document.root(), node->id, {}, parentTransform);
        const ImVec2 mouseParentLocal = inverseTransformPoint(parentTransform, mouseWorld);
        node->position.x = m_dragStartNodePosition.x + (mouseParentLocal.x - m_dragStartWorld.x);
        node->position.y = m_dragStartNodePosition.y + (mouseParentLocal.y - m_dragStartWorld.y);
        document.markDirty();
        return true;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        cancel(document);

    return false;
}

void NodeSelectCanvasTool::drawOverlay(EditorContext&, SceneDocument& document, const SceneCanvasContext& canvas) const
{
    SceneAffine selectedTransform;
    const SceneNode* selected = findSceneNodeWithTransform(document.root(), document.selectedNodeId(), {}, selectedTransform);
    if (!selected || selected->id == "root" || !selected->visible || !canvas.drawList)
        return;

    const ImU32 outline = selected->locked ? IM_COL32(255, 112, 112, 255) : IM_COL32(255, 204, 64, 255);
    const std::array<ImVec2, 4> worldCorners = sceneNodeWorldCorners(*selected, selectedTransform);
    ImVec2 screenCorners[4] = {};
    for (int index = 0; index < 4; ++index)
        screenCorners[index] = canvas.worldToScreen(worldCorners[index]);
    canvas.drawList->AddPolyline(screenCorners, 4, outline, ImDrawFlags_Closed, 2.0f);

    const SceneNodeSizeEditPolicy policy = sizeEditPolicyForNode(*selected);
    const bool resizeEnabled = !selected->locked && (policy.widthEditable || policy.heightEditable);
    if (resizeEnabled)
    {
        for (const ImVec2& point : screenCorners)
            drawControlPoint(*canvas.drawList, point, true);
    }

    const ImVec2 nodeOrigin = canvas.worldToScreen(transformPoint(
        selectedTransform, ImVec2(selected->anchor.x * selected->size.width, selected->anchor.y * selected->size.height)));
    canvas.drawList->AddCircleFilled(nodeOrigin, 4.0f, outline);
}

void NodeSelectCanvasTool::cancel(SceneDocument& document) const
{
    if (!m_draggingNodeId.empty() || !m_resizingNodeId.empty())
        document.commitUndoTransaction();
    reset();
}

void NodeSelectCanvasTool::reset() const
{
    m_draggingNodeId.clear();
    m_resizingNodeId.clear();
    m_resizeNodeHandleIndex = -1;
}
}  // namespace editor
