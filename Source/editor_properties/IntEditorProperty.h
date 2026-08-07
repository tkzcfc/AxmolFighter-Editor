#pragma once

#include "editor_properties/EditorProperty.h"

namespace editor
{
class IntEditorProperty final : public EditorProperty
{
public:
    IntEditorProperty(std::string id, std::string label, int& value, int minValue, int maxValue);
    void draw(EditorPropertyContext& context) override;

private:
    int& m_value;
    int m_minValue = 0;
    int m_maxValue = 0;
};
}  // namespace editor
