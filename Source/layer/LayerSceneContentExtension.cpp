#include "layer/LayerSceneContentExtension.h"

#include "core/EditorContext.h"
#include "documents/LayerDocument.h"
#include "layer/LayerPluginEditor.h"

namespace editor
{
SceneContentExtensionHooks makeLayerSceneContentExtensionHooks(LayerDocument& document)
{
    SceneContentExtensionHooks hooks;
    hooks.drawToolbar = [&document](EditorContext& context, SceneDocument& sceneDocument, SceneCanvasToolbarContext& toolbar) {
        for (const std::unique_ptr<LayerPluginEditor>& plugin : LayerPluginEditorRegistry::instance().editors())
            plugin->drawContentToolbar(context, sceneDocument, toolbar);
    };
    hooks.handleCanvasInteraction = [&document](EditorContext& context, SceneDocument& sceneDocument, const SceneCanvasContext& canvas) {
        for (const std::unique_ptr<LayerPluginEditor>& plugin : LayerPluginEditorRegistry::instance().editors())
        {
            if (plugin->handleCanvasInteraction(context, sceneDocument, canvas))
                return true;
        }
        return false;
    };
    hooks.drawCanvasOverlay = [&document](EditorContext& context, SceneDocument& sceneDocument, const SceneCanvasContext& canvas) {
        for (const std::unique_ptr<LayerPluginEditor>& plugin : LayerPluginEditorRegistry::instance().editors())
            plugin->drawCanvasOverlay(context, sceneDocument, canvas);
    };
    hooks.deleteSelection = [&document](SceneDocument&) {
        if (!document.hasShapeSelection())
            return false;
        document.deleteShape(document.selectedShapeId());
        return true;
    };
    hooks.moveSelection = [&document](SceneDocument&, const SceneVec2& delta) {
        if (delta.x == 0.0f && delta.y == 0.0f)
            return false;

        LayerShape* shape = document.selectedShape();
        if (!shape || shape->locked)
            return false;

        shape->position.x += delta.x;
        shape->position.y += delta.y;
        document.markDirty();
        return true;
    };
    return hooks;
}
}  // namespace editor
