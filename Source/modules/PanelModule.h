#pragma once

#include "modules/IEditorModule.h"

namespace editor
{
class PanelModule : public IEditorModule
{
public:
    explicit PanelModule(const char* title);

    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

protected:
    virtual void drawContent(EditorContext& context) = 0;

private:
    const char* m_title;
};
}  // namespace editor
