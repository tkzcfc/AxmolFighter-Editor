#include "document_editors/BoxDocumentEditor.h"

#include "combat/BoxEditorSessionService.h"
#include "core/EditorContext.h"
#include "documents/BoxDocument.h"

namespace editor
{
namespace
{
constexpr const char* kBoxTimelinePanelId = "Box Timeline";
}

std::type_index BoxDocumentEditor::documentType() const
{
    return typeid(BoxDocument);
}

void BoxDocumentEditor::update(EditorContext& context, IEditorDocument& document, float deltaTime)
{
    auto session = context.services().get<BoxEditorSessionService>();
    if (!session)
        return;
    auto& boxDocument = static_cast<BoxDocument&>(document);
    session->bind(boxDocument, context.settings());
    session->update(deltaTime);
}

void BoxDocumentEditor::drawContent(EditorContext& context, IEditorDocument& document)
{
    m_editor.drawContent(context, static_cast<BoxDocument&>(document));
}

void BoxDocumentEditor::drawInspector(EditorContext& context, IEditorDocument& document)
{
    m_editor.drawInspector(context, static_cast<BoxDocument&>(document));
}

void BoxDocumentEditor::onDeactivate(EditorContext& context)
{
    m_editor.reset();
    if (auto session = context.services().get<BoxEditorSessionService>())
        session->clear();
}

const std::vector<DocumentBottomPanelDescriptor>& BoxDocumentEditor::bottomPanels() const
{
    static const std::vector<DocumentBottomPanelDescriptor> kPanels = {
        {kBoxTimelinePanelId, kBoxTimelinePanelId, true}};
    return kPanels;
}

void BoxDocumentEditor::drawBottomPanel(EditorContext& context,
                                        IEditorDocument& document,
                                        std::string_view panelId)
{
    if (panelId == kBoxTimelinePanelId)
        m_timeline.draw(context, static_cast<BoxDocument&>(document));
}
}  // namespace editor
