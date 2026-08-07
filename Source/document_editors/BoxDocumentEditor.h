#pragma once

#include "combat/BoxEditor.h"
#include "document_editors/IDocumentEditor.h"
#include "modules/BoxTimelineModule.h"

namespace editor
{
class BoxDocumentEditor final : public IDocumentEditor
{
public:
    std::type_index documentType() const override;
    void update(EditorContext& context, IEditorDocument& document, float deltaTime) override;
    void drawContent(EditorContext& context, IEditorDocument& document) override;
    void drawInspector(EditorContext& context, IEditorDocument& document) override;
    void onDeactivate(EditorContext& context) override;
    const std::vector<DocumentBottomPanelDescriptor>& bottomPanels() const override;
    void drawBottomPanel(EditorContext& context, IEditorDocument& document, std::string_view panelId) override;

private:
    BoxEditor m_editor;
    BoxTimelineEditor m_timeline;
};
}  // namespace editor
