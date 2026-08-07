#pragma once

#include "core/IService.h"
#include "document_editors/IDocumentEditor.h"

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace editor
{
class IEditorDocument;

class DocumentEditorRegistry final : public IService
{
public:
    bool registerEditor(std::unique_ptr<IDocumentEditor> editor, std::string& error);

    IDocumentEditor* find(IEditorDocument& document);
    const IDocumentEditor* find(const IEditorDocument& document) const;
    IDocumentEditor* findPanelOwner(const std::string& panelId);
    const IDocumentEditor* findPanelOwner(const std::string& panelId) const;
    const std::vector<DocumentBottomPanelDescriptor>& bottomPanels() const;

private:
    std::unordered_map<std::type_index, std::unique_ptr<IDocumentEditor>> m_editors;
    std::unordered_map<std::string, IDocumentEditor*> m_panelOwners;
    std::vector<DocumentBottomPanelDescriptor> m_bottomPanels;
};
}  // namespace editor
