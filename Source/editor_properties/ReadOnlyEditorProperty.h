#pragma once

#include "editor_properties/EditorProperty.h"

namespace editor
{
class ReadOnlyEditorProperty final : public EditorProperty
{
public:
    ReadOnlyEditorProperty(std::string id, std::string label, std::string value);
    void draw(EditorPropertyContext& context) override;

private:
    std::string m_value;
};
}  // namespace editor
