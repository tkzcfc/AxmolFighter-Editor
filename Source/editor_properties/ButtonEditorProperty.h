#pragma once

#include "editor_properties/EditorProperty.h"

#include <functional>

namespace editor
{
class ButtonEditorProperty final : public EditorProperty
{
public:
    using Callback = std::function<void(EditorPropertyContext&)>;

    ButtonEditorProperty(std::string id, std::string label, Callback onClicked);
    void draw(EditorPropertyContext& context) override;

private:
    Callback m_onClicked;
};
}  // namespace editor
