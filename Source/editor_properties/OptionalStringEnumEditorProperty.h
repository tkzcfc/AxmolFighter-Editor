#pragma once

#include "editor_properties/EditorProperty.h"

#include <functional>
#include <string>
#include <vector>

namespace editor
{
class OptionalStringEnumEditorProperty final : public EditorProperty
{
public:
    using Callback = std::function<void(EditorPropertyContext&)>;

    OptionalStringEnumEditorProperty(std::string id,
                                    std::string label,
                                    std::string& value,
                                    std::vector<std::string> items,
                                    Callback onChanged = {});
    void draw(EditorPropertyContext& context) override;

private:
    std::string& m_value;
    std::vector<std::string> m_items;
    Callback m_onChanged;
};
}  // namespace editor
