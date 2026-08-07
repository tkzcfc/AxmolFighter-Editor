#pragma once

#include "document_editors/IDocumentEditor.h"
#include "motion/MotionEditor.h"

namespace editor
{
class MotionDocumentEditor final : public IDocumentEditor
{
public:
    std::type_index documentType() const override;
    void update(EditorContext& context, IEditorDocument& document, float deltaTime) override;
    void drawContent(EditorContext& context, IEditorDocument& document) override;
    void drawInspector(EditorContext& context, IEditorDocument& document) override;
    void onDeactivate(EditorContext& context) override;
    void drawBottomPanel(EditorContext& context, IEditorDocument& document, std::string_view panelId) override;

private:
    MotionEditor m_editor;
};
}  // namespace editor
