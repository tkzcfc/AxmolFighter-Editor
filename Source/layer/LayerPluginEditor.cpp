#include "layer/LayerPluginEditor.h"

#include "documents/LayerDocument.h"
#include "layer/ShapeLayerPluginEditor.h"

namespace editor
{
void LayerPluginEditor::drawContentToolbar(EditorContext& context,
                                           SceneDocument& document,
                                           SceneCanvasToolbarContext& toolbar) const
{
    if (auto* layerDocument = dynamic_cast<LayerDocument*>(&document))
        drawLayerContentToolbar(context, *layerDocument, toolbar);
}

bool LayerPluginEditor::handleCanvasInteraction(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const
{
    if (auto* layerDocument = dynamic_cast<LayerDocument*>(&document))
        return handleLayerCanvasInteraction(context, *layerDocument, canvas);
    return false;
}

void LayerPluginEditor::drawCanvasOverlay(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const
{
    if (auto* layerDocument = dynamic_cast<LayerDocument*>(&document))
        drawLayerCanvasOverlay(context, *layerDocument, canvas);
}

void LayerPluginEditor::drawLayerContentToolbar(EditorContext&, LayerDocument&, SceneCanvasToolbarContext&) const {}

bool LayerPluginEditor::handleLayerCanvasInteraction(EditorContext&, LayerDocument&, const SceneCanvasContext&) const
{
    return false;
}

void LayerPluginEditor::drawLayerCanvasOverlay(EditorContext&, LayerDocument&, const SceneCanvasContext&) const {}

LayerPluginEditorRegistry::LayerPluginEditorRegistry()
{
    m_editors.push_back(std::make_unique<ShapeLayerPluginEditor>());
}

const LayerPluginEditorRegistry& LayerPluginEditorRegistry::instance()
{
    static LayerPluginEditorRegistry registry;
    return registry;
}

const LayerPluginEditor* LayerPluginEditorRegistry::find(std::string_view pluginId) const
{
    for (const std::unique_ptr<LayerPluginEditor>& editor : m_editors)
    {
        if (editor->pluginId() == pluginId)
            return editor.get();
    }
    return nullptr;
}

const std::vector<std::unique_ptr<LayerPluginEditor>>& LayerPluginEditorRegistry::editors() const
{
    return m_editors;
}
}  // namespace editor
