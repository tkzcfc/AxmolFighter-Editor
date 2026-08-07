#pragma once

#include "scene/node_editors/BaseNodeEditor.h"

#include <string>

namespace editor
{
class FrameAnimationNodeEditor final : public BaseNodeEditor
{
public:
    std::string_view typeId() const override;
    std::string_view displayName() const override;
    std::string_view defaultName() const override;

    void initializeNode(SceneNode& node) const override;
    void readCustomData(SceneNode& node, const rapidjson::Value& value) const override;
    void writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                         const SceneNode& node) const override;
    void appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const override;
    void drawInspector(SceneDocument& document, SceneNode& node, std::string& activeEditKey) const override;
    std::tuple<std::string, ax::Node*> createEngineNode(const SceneNode& node,
                                                        const SceneNodeRuntimeContext& context) const override;
    std::string engineSignature(const SceneNode& node, const SceneNodeRuntimeContext& context) const override;
    bool updateEngineNode(ax::Node& runtimeNode,
                          const SceneNode& node,
                          const SceneNodeRuntimeContext& context) const override;

private:
    std::string defaultBlendSrc(const SceneNode& node) const;

    mutable std::string m_previewVariableKeyEdit;
    mutable std::string m_previewVariableKeyBuffer;
    mutable std::string m_previewVariableError;
    mutable std::string m_blendDefaultCacheKey;
    mutable std::string m_blendDefaultSrc = "SRC_ALPHA";
};
}  // namespace editor
