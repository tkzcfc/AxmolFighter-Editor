#pragma once

#include "editor_properties/EditorProperty.h"

#include <functional>

namespace editor
{
class BoolEditorProperty final : public EditorProperty
{
public:
    using Callback = std::function<void(EditorPropertyContext&)>;

    BoolEditorProperty(std::string id,
                      std::string label,
                      bool& value,
                      bool disableWhenNodeLocked = true,
                      Callback onChanged = {});
    void draw(EditorPropertyContext& context) override;

private:
    bool& m_value;
    Callback m_onChanged;
};
}  // namespace editor
