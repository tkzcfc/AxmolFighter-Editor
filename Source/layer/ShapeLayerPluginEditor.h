#pragma once

#include "documents/LayerDocument.h"
#include "layer/LayerPluginEditor.h"
#include "layer/ShapeEditCanvasTool.h"

#include <string>

namespace editor
{
class ShapeLayerPluginEditor final : public LayerPluginEditor
{
public:
    static constexpr std::string_view kContentToolId = ShapeEditCanvasTool::kId;

    std::string_view pluginId() const override;
    std::string_view displayName() const override;
    void drawLayerContentToolbar(EditorContext& context, LayerDocument& document, SceneCanvasToolbarContext& toolbar) const override;
    bool handleLayerCanvasInteraction(EditorContext& context, LayerDocument& document, const SceneCanvasContext& canvas) const override;
    void drawLayerCanvasOverlay(EditorContext& context, LayerDocument& document, const SceneCanvasContext& canvas) const override;
    void drawList(EditorContext& context, LayerDocument& document) const override;
    void drawInspector(LayerDocument& document, std::string& activeEditKey) const override;

private:
    mutable ShapeEditCanvasTool m_canvasTool;
};
}  // namespace editor
