#include "scene/node_editors/SceneNodeEditor.h"

#include "core/SignatureUtils.h"
#include "scene/node_editors/LabelNodeEditor.h"
#include "scene/node_editors/BaseNodeEditor.h"
#include "editor_properties/EditorProperty.h"
#include "scene/node_editors/ObjectNodeEditor.h"
#include "scene/node_editors/FrameAnimationNodeEditor.h"
#include "scene/node_editors/SpineNodeEditor.h"
#include "scene/node_editors/SpriteNodeEditor.h"
#include "scene/SceneDocument.h"

namespace editor
{
void SceneNodeEditor::initializeNode(SceneNode& node) const
{
    node.type = std::string(typeId());
    node.name = std::string(defaultName());
    node.size = {100.0f, 100.0f};
}

void SceneNodeEditor::readCustomData(SceneNode&, const rapidjson::Value&) const {}

void SceneNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>&, const SceneNode&) const {}

void SceneNodeEditor::appendPropertyGroups(SceneNode&, std::vector<EditorPropertyGroup>&) const {}

void SceneNodeEditor::drawInspector(SceneDocument& document, SceneNode& node, std::string& activeEditKey) const
{
    std::vector<EditorPropertyGroup> groups;
    appendPropertyGroups(node, groups);
    EditorPropertyContext context{nullptr, document, node, activeEditKey};
    drawEditorPropertyGroups(context, groups);
}

bool SceneNodeEditor::canHaveChildren() const
{
    return true;
}

SceneNodeSizeEditPolicy SceneNodeEditor::sizeEditPolicy(const SceneNode&) const
{
    return {};
}

bool SceneNodeEditor::afterCanvasResize(SceneNode&) const
{
    return false;
}

std::tuple<std::string, ax::Node*> SceneNodeEditor::createEngineNode(const SceneNode& node) const
{
    return createEngineNode(node, {});
}

std::string SceneNodeEditor::engineSignature(const SceneNode& node, const SceneNodeRuntimeContext&) const
{
    return SignatureBuilder().appendRaw(typeId()).appendString("type", node.type).hash();
}

bool SceneNodeEditor::updateEngineNode(ax::Node&, const SceneNode&, const SceneNodeRuntimeContext&) const
{
    return false;
}

SceneNodeEditorRegistry::SceneNodeEditorRegistry()
{
    m_editors.push_back(std::make_unique<BaseNodeEditor>());
    m_editors.push_back(std::make_unique<ObjectNodeEditor>());
    m_editors.push_back(std::make_unique<SpriteNodeEditor>());
    m_editors.push_back(std::make_unique<LabelNodeEditor>());
    m_editors.push_back(std::make_unique<SpineNodeEditor>());
    m_editors.push_back(std::make_unique<FrameAnimationNodeEditor>());
}

const SceneNodeEditorRegistry& SceneNodeEditorRegistry::instance()
{
    static SceneNodeEditorRegistry registry;
    return registry;
}

const SceneNodeEditor* SceneNodeEditorRegistry::find(std::string_view typeId) const
{
    for (const std::unique_ptr<SceneNodeEditor>& editor : m_editors)
    {
        if (editor->typeId() == typeId)
            return editor.get();
    }
    return nullptr;
}

const SceneNodeEditor& SceneNodeEditorRegistry::fallbackEditor() const
{
    return *m_editors.front();
}

const std::vector<std::unique_ptr<SceneNodeEditor>>& SceneNodeEditorRegistry::editors() const
{
    return m_editors;
}
}  // namespace editor
