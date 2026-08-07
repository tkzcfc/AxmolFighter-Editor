#pragma once

namespace editor
{
class EditorContext;

class IEditorModule
{
public:
    virtual ~IEditorModule() = default;

    virtual void onAttach(EditorContext& context) = 0;
    virtual void onDetach(EditorContext& context) = 0;
    virtual void onUpdate(EditorContext& context, float deltaTime) = 0;
    virtual void onImGuiRender(EditorContext& context) = 0;
};
}  // namespace editor
