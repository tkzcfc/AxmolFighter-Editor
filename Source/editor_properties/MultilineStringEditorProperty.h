#pragma once

#include "editor_properties/EditorProperty.h"

#include <functional>

namespace editor
{
class MultilineStringEditorProperty final : public EditorProperty
{
public:
    using Callback = std::function<void(EditorPropertyContext&)>;

    MultilineStringEditorProperty(std::string id, std::string label, std::string& value, Callback onChanged = {}, Callback afterEdit = {});
    void draw(EditorPropertyContext& context) override;

private:
    std::string& m_value;
    Callback m_onChanged;
    Callback m_afterEdit;
};
}  // namespace editor
