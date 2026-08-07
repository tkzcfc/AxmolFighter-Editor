#include "document_editors/IDocumentEditor.h"

namespace editor
{
const std::vector<DocumentBottomPanelDescriptor>& IDocumentEditor::bottomPanels() const
{
    static const std::vector<DocumentBottomPanelDescriptor> kNoPanels;
    return kNoPanels;
}
}  // namespace editor
