#pragma once

#include "editor_properties/EditorProperty.h"
#include "scene/node_editors/SceneNodeEditor.h"

namespace editor
{
class SizeEditorProperty final : public EditorProperty
{
public:
    SizeEditorProperty(std::string id, std::string label, SceneSize& value, SceneNodeSizeEditPolicy policy);
    void draw(EditorPropertyContext& context) override;

private:
    SceneSize& m_value;
    SceneNodeSizeEditPolicy m_policy;
};
}  // namespace editor
