#pragma once

#include "scene/ScenePluginEditor.h"

#include <memory>
#include <string_view>
#include <vector>

namespace editor
{
class EditorContext;
class LayerDocument;

class LayerPluginEditor : public ScenePluginEditor
{
public:
    virtual ~LayerPluginEditor() = default;

    void drawContentToolbar(EditorContext& context, SceneDocument& document, SceneCanvasToolbarContext& toolbar) const override;
    bool handleCanvasInteraction(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const override;
    void drawCanvasOverlay(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const override;
    virtual void drawLayerContentToolbar(EditorContext& context, LayerDocument& document, SceneCanvasToolbarContext& toolbar) const;
    virtual bool handleLayerCanvasInteraction(EditorContext& context, LayerDocument& document, const SceneCanvasContext& canvas) const;
    virtual void drawLayerCanvasOverlay(EditorContext& context, LayerDocument& document, const SceneCanvasContext& canvas) const;
    virtual void drawList(EditorContext& context, LayerDocument& document) const = 0;
    virtual void drawInspector(LayerDocument& document, std::string& activeEditKey) const = 0;
};

class LayerPluginEditorRegistry
{
public:
    static const LayerPluginEditorRegistry& instance();

    const LayerPluginEditor* find(std::string_view pluginId) const;
    const std::vector<std::unique_ptr<LayerPluginEditor>>& editors() const;

private:
    LayerPluginEditorRegistry();

    std::vector<std::unique_ptr<LayerPluginEditor>> m_editors;
};
}  // namespace editor
