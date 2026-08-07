#pragma once

#include "scene/SceneTypes.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace ax
{
class Node;
}

namespace editor
{
class SceneDocument;
struct EditorPropertyGroup;

struct SceneNodeRuntimeContext
{
    int depth = 0;
};

struct SceneNodeSizeEditPolicy
{
    bool widthEditable = true;
    bool heightEditable = true;
    std::string reason;
};

class SceneNodeEditor
{
public:
    virtual ~SceneNodeEditor() = default;

    virtual std::string_view typeId() const = 0;
    virtual std::string_view displayName() const = 0;
    virtual std::string_view defaultName() const = 0;
    virtual void initializeNode(SceneNode& node) const;
    virtual void readCustomData(SceneNode& node, const rapidjson::Value& value) const;
    virtual void writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                 const SceneNode& node) const;
    virtual void appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const;
    virtual void drawInspector(SceneDocument& document, SceneNode& node, std::string& activeEditKey) const;
    virtual bool canHaveChildren() const;
    virtual SceneNodeSizeEditPolicy sizeEditPolicy(const SceneNode& node) const;
    virtual bool afterCanvasResize(SceneNode& node) const;

    std::tuple<std::string, ax::Node*> createEngineNode(const SceneNode& node) const;
    virtual std::tuple<std::string, ax::Node*> createEngineNode(const SceneNode& node,
                                                               const SceneNodeRuntimeContext& context) const = 0;
    virtual std::string engineSignature(const SceneNode& node, const SceneNodeRuntimeContext& context) const;
    virtual bool updateEngineNode(ax::Node& runtimeNode,
                                  const SceneNode& node,
                                  const SceneNodeRuntimeContext& context) const;
};

class SceneNodeEditorRegistry
{
public:
    static const SceneNodeEditorRegistry& instance();

    const SceneNodeEditor* find(std::string_view typeId) const;
    const SceneNodeEditor& fallbackEditor() const;
    const std::vector<std::unique_ptr<SceneNodeEditor>>& editors() const;

private:
    SceneNodeEditorRegistry();

    std::vector<std::unique_ptr<SceneNodeEditor>> m_editors;
};

}  // namespace editor
