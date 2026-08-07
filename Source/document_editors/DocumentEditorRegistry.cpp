#include "document_editors/DocumentEditorRegistry.h"

#include "documents/IEditorDocument.h"

namespace editor
{
bool DocumentEditorRegistry::registerEditor(std::unique_ptr<IDocumentEditor> editor, std::string& error)
{
    error.clear();
    if (!editor)
    {
        error = "Document editor is null.";
        return false;
    }
    const std::type_index type = editor->documentType();
    if (m_editors.contains(type))
    {
        error = "A document editor is already registered for this document type.";
        return false;
    }
    for (const DocumentBottomPanelDescriptor& panel : editor->bottomPanels())
    {
        if (panel.panelId.empty() || panel.title.empty())
        {
            error = "Document bottom panel id and title are required.";
            return false;
        }
        if (m_panelOwners.contains(panel.panelId))
        {
            error = "A document bottom panel is already registered with id: " + panel.panelId;
            return false;
        }
    }

    IDocumentEditor* rawEditor = editor.get();
    for (const DocumentBottomPanelDescriptor& panel : editor->bottomPanels())
    {
        m_panelOwners.emplace(panel.panelId, rawEditor);
        m_bottomPanels.push_back(panel);
    }
    m_editors.emplace(type, std::move(editor));
    return true;
}

IDocumentEditor* DocumentEditorRegistry::find(IEditorDocument& document)
{
    const auto it = m_editors.find(std::type_index(typeid(document)));
    return it == m_editors.end() ? nullptr : it->second.get();
}

const IDocumentEditor* DocumentEditorRegistry::find(const IEditorDocument& document) const
{
    const auto it = m_editors.find(std::type_index(typeid(document)));
    return it == m_editors.end() ? nullptr : it->second.get();
}

IDocumentEditor* DocumentEditorRegistry::findPanelOwner(const std::string& panelId)
{
    const auto it = m_panelOwners.find(panelId);
    return it == m_panelOwners.end() ? nullptr : it->second;
}

const IDocumentEditor* DocumentEditorRegistry::findPanelOwner(const std::string& panelId) const
{
    const auto it = m_panelOwners.find(panelId);
    return it == m_panelOwners.end() ? nullptr : it->second;
}

const std::vector<DocumentBottomPanelDescriptor>& DocumentEditorRegistry::bottomPanels() const
{
    return m_bottomPanels;
}
}  // namespace editor
