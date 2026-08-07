#pragma once

#include "editor_properties/EditorProperty.h"

namespace editor
{
class ColorEditorProperty final : public EditorProperty
{
public:
    ColorEditorProperty(std::string id, std::string label, SceneColor& value);
    void draw(EditorPropertyContext& context) override;

private:
    SceneColor& m_value;
};
}  // namespace editor
