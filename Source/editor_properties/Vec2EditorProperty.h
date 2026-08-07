#pragma once

#include "editor_properties/EditorProperty.h"

namespace editor
{
class Vec2EditorProperty final : public EditorProperty
{
public:
    Vec2EditorProperty(std::string id, std::string label, SceneVec2& value);
    void draw(EditorPropertyContext& context) override;

private:
    SceneVec2& m_value;
};
}  // namespace editor
