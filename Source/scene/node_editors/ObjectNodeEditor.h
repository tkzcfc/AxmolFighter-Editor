#pragma once

#include "scene/node_editors/BaseNodeEditor.h"

namespace editor
{
class ObjectNodeEditor final : public BaseNodeEditor
{
public:
    std::string_view typeId() const override;
    std::string_view displayName() const override;
    std::string_view defaultName() const override;

    void initializeNode(SceneNode& node) const override;
    void readCustomData(SceneNode& node, const rapidjson::Value& value) const override;
    void writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const SceneNode& node) const override;
    void drawInspector(SceneDocument& document, SceneNode& node, std::string& activeEditKey) const override;
    bool canHaveChildren() const override;
    std::tuple<std::string, ax::Node*> createEngineNode(const SceneNode& node,
                                                        const SceneNodeRuntimeContext& context) const override;
    std::string engineSignature(const SceneNode& node, const SceneNodeRuntimeContext& context) const override;
    bool updateEngineNode(ax::Node& runtimeNode,
                          const SceneNode& node,
                          const SceneNodeRuntimeContext& context) const override;
};
}  // namespace editor
