#pragma once

#include "modules/IEditorModule.h"
#include "ui/EditorDockSpace.h"

namespace editor
{
class DockingLayoutModule : public IEditorModule
{
public:
    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

private:
    void buildDefaultLayout(EditorContext& context, ImGuiID dockSpaceId);
    bool shouldBuildLayout(EditorContext& context) const;

    EditorDockSpace m_dockSpace;
    bool m_layoutInitialized = false;
};
}  // namespace editor
