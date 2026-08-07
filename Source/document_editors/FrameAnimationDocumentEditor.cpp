#include "document_editors/FrameAnimationDocumentEditor.h"

#include "documents/AniDocument.h"

namespace editor
{
namespace
{
constexpr const char* kAnimationTimelinePanelId = "Animation Timeline";
}

std::type_index FrameAnimationDocumentEditor::documentType() const
{
    return typeid(AniDocument);
}

void FrameAnimationDocumentEditor::update(EditorContext&, IEditorDocument& document, float deltaTime)
{
    auto& aniDocument = static_cast<AniDocument&>(document);
    m_editor.prepare(aniDocument);
    m_editor.update(deltaTime);
}

void FrameAnimationDocumentEditor::drawContent(EditorContext& context, IEditorDocument& document)
{
    m_editor.drawContent(context, static_cast<AniDocument&>(document));
}

void FrameAnimationDocumentEditor::drawInspector(EditorContext&, IEditorDocument& document)
{
    m_editor.drawInspector(static_cast<AniDocument&>(document));
}

void FrameAnimationDocumentEditor::onDeactivate(EditorContext&)
{
    m_editor.reset();
}

const std::vector<DocumentBottomPanelDescriptor>& FrameAnimationDocumentEditor::bottomPanels() const
{
    static const std::vector<DocumentBottomPanelDescriptor> kPanels = {
        {kAnimationTimelinePanelId, kAnimationTimelinePanelId, true}};
    return kPanels;
}

void FrameAnimationDocumentEditor::drawBottomPanel(EditorContext& context,
                                                    IEditorDocument& document,
                                                    std::string_view panelId)
{
    if (panelId == kAnimationTimelinePanelId)
        m_editor.drawTimeline(context, static_cast<AniDocument&>(document));
}
}  // namespace editor
