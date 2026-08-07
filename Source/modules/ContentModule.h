#pragma once

#include "modules/PanelModule.h"
#include "scene/SceneContentEditor.h"

#include <cstddef>
#include <string>

namespace editor
{
class ContentModule : public PanelModule
{
public:
    ContentModule();
    ~ContentModule() override;

private:
    void onUpdate(EditorContext& context, float deltaTime) override;
    void drawContent(EditorContext& context) override;
    void drawDocumentTabs(EditorContext& context);
    void drawActiveDocument(EditorContext& context);
    void drawLayerDocument(EditorContext& context, class LayerDocument& document);
    void drawTextDocument(class TextDocument& document);

    std::string m_statusMessage;
    SceneContentEditor m_sceneContentEditor;
    std::size_t m_lastTabActiveIndex         = static_cast<std::size_t>(-1);
    std::size_t m_seenDocumentActiveRevision = 0;
};
}  // namespace editor
