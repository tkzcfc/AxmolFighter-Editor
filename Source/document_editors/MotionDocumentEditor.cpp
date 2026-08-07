#include "document_editors/MotionDocumentEditor.h"

#include "documents/MotionDocument.h"

namespace editor
{
std::type_index MotionDocumentEditor::documentType() const
{
    return typeid(MotionDocument);
}

void MotionDocumentEditor::update(EditorContext&, IEditorDocument&, float) {}

void MotionDocumentEditor::drawContent(EditorContext& context, IEditorDocument& document)
{
    m_editor.drawContent(context, static_cast<MotionDocument&>(document));
}

void MotionDocumentEditor::drawInspector(EditorContext& context, IEditorDocument& document)
{
    m_editor.drawInspector(context, static_cast<MotionDocument&>(document));
}

void MotionDocumentEditor::onDeactivate(EditorContext&)
{
    m_editor.reset();
}

void MotionDocumentEditor::drawBottomPanel(EditorContext&, IEditorDocument&, std::string_view) {}
}  // namespace editor
