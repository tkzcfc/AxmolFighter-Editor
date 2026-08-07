#include "modules/DocumentEditorPanelsModule.h"

#include "core/EditorContext.h"
#include "document_editors/DocumentEditorRegistry.h"
#include "documents/IEditorDocument.h"
#include "imgui.h"
#include "modules/PanelVisibilityService.h"

namespace editor
{
void DocumentEditorPanelsModule::onAttach(EditorContext& context)
{
    const auto registry = context.services().get<DocumentEditorRegistry>();
    const auto visibility = context.services().get<PanelVisibilityService>();
    if (!registry || !visibility)
        return;
    for (const DocumentBottomPanelDescriptor& panel : registry->bottomPanels())
    {
        if (panel.openByDefault)
            visibility->open(panel.panelId);
    }
}

void DocumentEditorPanelsModule::onDetach(EditorContext& context)
{
    if (m_activeEditor)
        m_activeEditor->onDeactivate(context);
    m_activeEditor = nullptr;
}

void DocumentEditorPanelsModule::onUpdate(EditorContext& context, float deltaTime)
{
    const auto registry = context.services().get<DocumentEditorRegistry>();
    IEditorDocument* document = context.documents().activeDocument();
    IDocumentEditor* editor = registry && document ? registry->find(*document) : nullptr;
    if (editor != m_activeEditor)
    {
        if (m_activeEditor)
            m_activeEditor->onDeactivate(context);
        m_activeEditor = editor;
    }
    if (editor && document)
        editor->update(context, *document, deltaTime);
}

void DocumentEditorPanelsModule::onImGuiRender(EditorContext& context)
{
    const auto registry = context.services().get<DocumentEditorRegistry>();
    const auto visibility = context.services().get<PanelVisibilityService>();
    if (!registry)
        return;

    IEditorDocument* document = context.documents().activeDocument();
    IDocumentEditor* activeEditor = document ? registry->find(*document) : nullptr;
    for (const DocumentBottomPanelDescriptor& panel : registry->bottomPanels())
    {
        if (visibility && !visibility->isOpen(panel.panelId))
            continue;
        bool open = true;
        if (!ImGui::Begin(panel.title.c_str(), &open))
        {
            ImGui::End();
            if (!open && visibility)
                visibility->close(panel.panelId);
            continue;
        }
        IDocumentEditor* owner = registry->findPanelOwner(panel.panelId);
        if (document && owner && owner == activeEditor)
            owner->drawBottomPanel(context, *document, panel.panelId);
        else
            ImGui::TextDisabled("Open the matching document type to edit this panel.");
        ImGui::End();
        if (!open && visibility)
            visibility->close(panel.panelId);
    }
}
}  // namespace editor
