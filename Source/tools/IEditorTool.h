#pragma once

#include <string_view>

namespace editor
{
class EditorContext;

class IEditorTool
{
public:
    virtual ~IEditorTool() = default;

    virtual std::string_view getId() const = 0;
    virtual std::string_view getName() const = 0;
    virtual void onActivate(EditorContext& context) = 0;
    virtual void onDeactivate(EditorContext& context) = 0;
    virtual void onUpdate(EditorContext& context, float deltaTime) = 0;
    virtual void onImGuiRender(EditorContext& context) = 0;
};
}  // namespace editor
