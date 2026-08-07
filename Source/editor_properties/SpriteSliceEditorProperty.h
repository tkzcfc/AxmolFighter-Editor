#pragma once

#include "editor_properties/EditorProperty.h"

namespace editor
{
class SpriteSliceEditorProperty final : public EditorProperty
{
public:
    SpriteSliceEditorProperty(std::string id, std::string label);
    void draw(EditorPropertyContext& context) override;
};
}  // namespace editor
