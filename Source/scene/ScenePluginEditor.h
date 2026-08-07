#pragma once

#include "scene/canvas/SceneCanvas.h"
#include "scene/canvas/SceneCanvasToolbarContext.h"

#include <string_view>

namespace editor
{
class EditorContext;
class SceneDocument;

class ScenePluginEditor
{
public:
    virtual ~ScenePluginEditor() = default;

    virtual std::string_view pluginId() const = 0;
    virtual std::string_view displayName() const = 0;
    virtual void drawContentToolbar(EditorContext& context, SceneDocument& document, SceneCanvasToolbarContext& toolbar) const;
    virtual bool handleCanvasInteraction(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const;
    virtual void drawCanvasOverlay(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const;
};
}  // namespace editor
