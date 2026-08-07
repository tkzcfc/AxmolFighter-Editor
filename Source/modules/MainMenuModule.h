#pragma once

#include "modules/IEditorModule.h"

#include <string>

namespace editor
{
class MainMenuModule : public IEditorModule
{
public:
    void onAttach(EditorContext& context) override;
    void onDetach(EditorContext& context) override;
    void onUpdate(EditorContext& context, float deltaTime) override;
    void onImGuiRender(EditorContext& context) override;

private:
    void saveActiveDocument(EditorContext& context);
    void saveAllDocuments(EditorContext& context);
    void undoActiveDocument(EditorContext& context);
    void redoActiveDocument(EditorContext& context);
    void reloadActiveDocument(EditorContext& context);
    void closeActiveDocument(EditorContext& context);
    void closeAllCleanDocuments(EditorContext& context);

    std::string m_statusMessage;
};
}  // namespace editor
