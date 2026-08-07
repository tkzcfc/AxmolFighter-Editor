#include "scene/ScenePluginEditor.h"

namespace editor
{
void ScenePluginEditor::drawContentToolbar(EditorContext&, SceneDocument&, SceneCanvasToolbarContext&) const {}

bool ScenePluginEditor::handleCanvasInteraction(EditorContext&, SceneDocument&, const SceneCanvasContext&) const
{
    return false;
}

void ScenePluginEditor::drawCanvasOverlay(EditorContext&, SceneDocument&, const SceneCanvasContext&) const {}
}  // namespace editor
