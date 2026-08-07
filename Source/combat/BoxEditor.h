#pragma once

#include "combat/CombatBoxTypes.h"
#include "scene/canvas/SceneCanvas.h"

#include <array>
#include <cstdint>
#include <string>

namespace editor
{
class BoxDocument;
class EditorContext;

class BoxEditor
{
public:
    void drawContent(EditorContext& context, BoxDocument& document);
    void drawInspector(EditorContext& context, BoxDocument& document);
    void reset();

private:
    void drawCanvas(EditorContext& context, BoxDocument& document, float height);
    void drawPreviewControls(EditorContext& context, BoxDocument& document);

    SceneCanvasViewState m_viewState;
    std::string m_documentPath;
    bool m_dragging = false;
    bool m_resizingX = false;
    bool m_resizingZ = false;
    bool m_resizingXNegative = false;
    bool m_resizingZNegative = false;
    int m_dragTrack = -1;
    int m_dragKey = -1;
    ImVec2 m_dragStartWorld;
    CombatBox m_dragOriginalBox;
    int m_shiftKeysMs = 0;
};
}  // namespace editor
