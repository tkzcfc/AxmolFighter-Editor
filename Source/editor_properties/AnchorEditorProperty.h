#pragma once

#include "editor_properties/EditorProperty.h"

namespace editor
{
class AnchorEditorProperty final : public EditorProperty
{
public:
    AnchorEditorProperty(std::string id, std::string label, SceneVec2& value);
    void draw(EditorPropertyContext& context) override;

private:
    bool setAnchor(EditorPropertyContext& context, const SceneVec2& value);

    SceneVec2& m_value;
};
}  // namespace editor
