#pragma once

#include "modules/IEditorModule.h"

namespace editor
{
class IDocumentEditor;

class DocumentEditorPanelsModule final : public IEditorModule
{
public:
    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

private:
    IDocumentEditor* m_activeEditor = nullptr;
};
}  // namespace editor
