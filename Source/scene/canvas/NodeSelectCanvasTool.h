#pragma once

#include "scene/canvas/SceneCanvasTool.h"
#include "scene/SceneTypes.h"

#include <string>

namespace editor
{
class NodeSelectCanvasTool final : public SceneCanvasTool
{
public:
    static constexpr std::string_view kId = "node.select";

    std::string_view id() const override;
    std::string_view displayName() const override;
    bool handleInput(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const override;
    void drawOverlay(EditorContext& context, SceneDocument& document, const SceneCanvasContext& canvas) const override;
    void cancel(SceneDocument& document) const;
    void reset() const;

private:
    mutable std::string m_draggingNodeId;
    mutable std::string m_resizingNodeId;
    mutable int m_resizeNodeHandleIndex = -1;
    mutable SceneVec2 m_dragStartWorld;
    mutable SceneVec2 m_dragStartNodePosition;
    mutable SceneVec2 m_resizeFixedParent;
};
}  // namespace editor
