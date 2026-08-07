#pragma once

#include "documents/LayerDocument.h"
#include "scene/canvas/SceneCanvas.h"

#include <string>

namespace editor
{
class EditorContext;

class ShapeEditCanvasTool final
{
public:
    static constexpr std::string_view kId = "shape.edit";

    std::string_view id() const;
    std::string_view displayName() const;
    bool handleInput(EditorContext& context, LayerDocument& document, const SceneCanvasContext& canvas) const;
    void drawOverlay(EditorContext& context, LayerDocument& document, const SceneCanvasContext& canvas) const;
    void cancel(LayerDocument& document) const;
    void reset() const;

private:
    mutable std::string m_draggingShapeId;
    mutable std::string m_resizingShapeId;
    mutable int m_resizeHandleIndex = -1;
    mutable SceneVec2 m_dragStartWorld;
    mutable SceneVec2 m_dragStartShapePosition;
    mutable SceneVec2 m_resizeFixedWorld;
};
}  // namespace editor
