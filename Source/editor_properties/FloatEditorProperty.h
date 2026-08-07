#pragma once

#include "editor_properties/EditorProperty.h"

#include <functional>

namespace editor
{
class FloatEditorProperty final : public EditorProperty
{
public:
    using Callback = std::function<void(EditorPropertyContext&)>;

    FloatEditorProperty(std::string id, std::string label, float& value, Callback afterEdit = {});
    void draw(EditorPropertyContext& context) override;

private:
    float& m_value;
    Callback m_afterEdit;
};
}  // namespace editor
