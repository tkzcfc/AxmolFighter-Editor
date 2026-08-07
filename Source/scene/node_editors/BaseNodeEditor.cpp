#include "scene/node_editors/BaseNodeEditor.h"

#include "2d/DrawNode.h"
#include "core/SignatureUtils.h"
#include "imgui.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"
#include "editor_properties/AnchorEditorProperty.h"
#include "editor_properties/BoolEditorProperty.h"
#include "editor_properties/ColorEditorProperty.h"
#include "editor_properties/FloatEditorProperty.h"
#include "editor_properties/IntEditorProperty.h"
#include "editor_properties/ReadOnlyEditorProperty.h"
#include "editor_properties/SizeEditorProperty.h"
#include "editor_properties/StringEditorProperty.h"
#include "editor_properties/Vec2EditorProperty.h"

#include <algorithm>

namespace editor
{
namespace
{
ax::Color4F nodeFillColor(const SceneNode& node)
{
    return ax::Color4F(std::clamp(node.color.r, 0.0f, 1.0f),
                       std::clamp(node.color.g, 0.0f, 1.0f),
                       std::clamp(node.color.b, 0.0f, 1.0f),
                       std::clamp(node.color.a, 0.0f, 1.0f) * (std::clamp(node.opacity, 0, 255) / 255.0f));
}
}  // namespace

std::string_view BaseNodeEditor::typeId() const
{
    return "Node";
}

std::string_view BaseNodeEditor::displayName() const
{
    return "Node";
}

std::string_view BaseNodeEditor::defaultName() const
{
    return "Node";
}

void BaseNodeEditor::readCustomData(SceneNode& node, const rapidjson::Value& value) const
{
    if (value.IsObject() && value.HasMember("drawFill") && value["drawFill"].IsBool())
        node.node.drawFill = value["drawFill"].GetBool();
}

void BaseNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                     const SceneNode& node) const
{
    writer.Key("drawFill");
    writer.Bool(node.node.drawFill);
}

void BaseNodeEditor::appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const
{
    const std::string editPrefix = node.id + ":";
    EditorPropertyGroup group;
    group.label = "Node";
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "visible", "Visible", node.visible, false));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "locked", "Locked", node.locked, false));
    group.properties.push_back(std::make_unique<StringEditorProperty>(editPrefix + "name", "Name", node.name));
    group.properties.push_back(std::make_unique<StringEditorProperty>(editPrefix + "note", "Note", node.note));
    group.properties.push_back(std::make_unique<ReadOnlyEditorProperty>(editPrefix + "id", "ID", node.id));
    group.properties.push_back(std::make_unique<ReadOnlyEditorProperty>(editPrefix + "type", "Type", node.type));
    group.properties.push_back(std::make_unique<Vec2EditorProperty>(editPrefix + "position", "Position", node.position));
    group.properties.push_back(std::make_unique<FloatEditorProperty>(editPrefix + "positionZ", "Position Z", node.positionZ));
    group.properties.push_back(
        std::make_unique<SizeEditorProperty>(editPrefix + "size", "Size", node.size, sizeEditPolicy(node)));
    group.properties.push_back(std::make_unique<AnchorEditorProperty>(editPrefix + "anchor", "Anchor", node.anchor));
    group.properties.push_back(std::make_unique<Vec2EditorProperty>(editPrefix + "scale", "Scale", node.scale));
    group.properties.push_back(std::make_unique<FloatEditorProperty>(editPrefix + "rotation", "Rotation", node.rotation));
    group.properties.push_back(std::make_unique<Vec2EditorProperty>(editPrefix + "skew", "Skew", node.skew));
    group.properties.push_back(std::make_unique<ColorEditorProperty>(editPrefix + "color", "Color", node.color));
    group.properties.push_back(std::make_unique<IntEditorProperty>(editPrefix + "opacity", "Opacity", node.opacity, 0, 255));
    groups.push_back(std::move(group));

    if (typeId() == "Node")
    {
        EditorPropertyGroup drawGroup;
        drawGroup.label = "Node";
        drawGroup.properties.push_back(
            std::make_unique<BoolEditorProperty>(editPrefix + "drawFill", "Draw Fill", node.node.drawFill, false));
        groups.push_back(std::move(drawGroup));
    }
}

std::tuple<std::string, ax::Node*> BaseNodeEditor::createEngineNode(const SceneNode& node,
                                                                         const SceneNodeRuntimeContext&) const
{
    auto* drawNode = ax::DrawNode::create();
    if (!drawNode)
        return {"Failed to create Node draw placeholder", nullptr};

    if (node.node.drawFill)
    {
        const float width = std::max(1.0f, node.size.width);
        const float height = std::max(1.0f, node.size.height);
        drawNode->drawSolidRect(ax::Vec2(0.0f, 0.0f), ax::Vec2(width, height), nodeFillColor(node));
    }
    return {"", drawNode};
}

std::string BaseNodeEditor::engineSignature(const SceneNode&, const SceneNodeRuntimeContext&) const
{
    return SignatureBuilder().appendRaw("Node").hash();
}

bool BaseNodeEditor::updateEngineNode(ax::Node& runtimeNode,
                                           const SceneNode& node,
                                           const SceneNodeRuntimeContext&) const
{
    auto* drawNode = dynamic_cast<ax::DrawNode*>(&runtimeNode);
    if (!drawNode)
        return false;

    drawNode->clear();
    if (node.node.drawFill)
    {
        const float width = std::max(1.0f, node.size.width);
        const float height = std::max(1.0f, node.size.height);
        drawNode->drawSolidRect(ax::Vec2(0.0f, 0.0f), ax::Vec2(width, height), nodeFillColor(node));
    }
    return true;
}
}  // namespace editor
