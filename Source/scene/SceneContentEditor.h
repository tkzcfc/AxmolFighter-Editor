#pragma once

#include "scene/SceneDocument.h"
#include "scene/canvas/NodeSelectCanvasTool.h"
#include "scene/canvas/SceneCanvas.h"
#include "scene/canvas/SceneCanvasToolbarContext.h"
#include "scene/runtime/ScenePreviewRuntime.h"

#include <filesystem>
#include <functional>
#include <string>

namespace editor
{
class EditorContext;

struct SceneContentExtensionHooks
{
    std::function<void(EditorContext&, SceneDocument&, SceneCanvasToolbarContext&)> drawToolbar;
    std::function<bool(EditorContext&, SceneDocument&, const SceneCanvasContext&)> handleCanvasInteraction;
    std::function<void(EditorContext&, SceneDocument&, const SceneCanvasContext&)> drawCanvasOverlay;
    std::function<bool(SceneDocument&)> deleteSelection;
    std::function<bool(SceneDocument&, const SceneVec2&)> moveSelection;
};

class SceneContentEditor final
{
public:
    void update(float deltaTime);
    void reset();
    void draw(EditorContext& context,
              SceneDocument& document,
              const std::string& displayName,
              const std::filesystem::path& documentKey,
              const SceneContentExtensionHooks& extensionHooks);

private:
    void drawLeftToolbar(EditorContext& context,
                         SceneDocument& document,
                         const SceneContentExtensionHooks& extensionHooks,
                         float height);
    void drawAlignmentToolbar(SceneDocument& document);
    void commitKeyboardMoveTransaction(SceneDocument& document);
    void handleNodeClipboardShortcuts(EditorContext& context, SceneDocument& document, const std::filesystem::path& documentKey);

    std::string m_activeToolId = "node.select";
    float m_zoom = 1.0f;
    ImVec2 m_pan;
    ScenePreviewRuntime m_previewRuntime;
    NodeSelectCanvasTool m_nodeSelectTool;
    bool m_keyboardMoveTransactionActive = false;
};
}  // namespace editor
