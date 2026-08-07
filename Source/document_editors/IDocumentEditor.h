#pragma once

#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

namespace editor
{
class EditorContext;
class IEditorDocument;

struct DocumentBottomPanelDescriptor
{
    std::string panelId;
    std::string title;
    bool openByDefault = true;
};

class IDocumentEditor
{
public:
    virtual ~IDocumentEditor() = default;

    virtual std::type_index documentType() const = 0;
    virtual void update(EditorContext& context, IEditorDocument& document, float deltaTime) = 0;
    virtual void drawContent(EditorContext& context, IEditorDocument& document) = 0;
    virtual void drawInspector(EditorContext& context, IEditorDocument& document) = 0;
    virtual void onDeactivate(EditorContext&) {}

    virtual const std::vector<DocumentBottomPanelDescriptor>& bottomPanels() const;
    virtual void drawBottomPanel(EditorContext& context, IEditorDocument& document, std::string_view panelId) = 0;
};
}  // namespace editor
