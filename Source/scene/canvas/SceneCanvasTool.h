#pragma once

#include "scene/canvas/SceneCanvas.h"

#include <string>
#include <string_view>

namespace editor
{
class EditorContext;
class SceneDocument;

class SceneCanvasTool
{
public:
    virtual ~SceneCanvasTool() = default;

    virtual std::string_view id() const = 0;
    virtual std::string_view displayName() const = 0;
    virtual const char* icon() const;
    virtual bool isAvailable(const SceneDocument& document) const;
    virtual void drawToolbarButton(std::string& activeToolId) const;
    virtual void onActivate(EditorContext& context, SceneDocument& document) const;
    virtual void onDeactivate(EditorContext& context, SceneDocument& document) const;
    virtual bool handleInput(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const;
    virtual void drawOverlay(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const;
};
}  // namespace editor
