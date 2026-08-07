#pragma once

#include "modules/IEditorModule.h"

namespace editor
{
class ModalDialogModule final : public IEditorModule
{
public:
    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;
};
}  // namespace editor
