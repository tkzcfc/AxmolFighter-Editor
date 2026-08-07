#pragma once

#include "animation/AnimationTypes.h"
#include "scene/canvas/SceneCanvas.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace ax
{
class Sprite;
class SpriteFrame;
}  // namespace ax

namespace editor
{
class AniDocument;
class EditorContext;
struct AnimationFrameData;

class FrameAnimationEditor
{
public:
    FrameAnimationEditor();
    ~FrameAnimationEditor();

    void update(float deltaTime);
    void prepare(AniDocument& document);
    void drawContent(EditorContext& context, AniDocument& document);
    void drawInspector(AniDocument& document);
    void drawTimeline(EditorContext& context, AniDocument& document);
    void reset();

private:
    ax::SpriteFrame* resolveFrame(const AniDocument& document,
                                  const AnimationFrameData& frame,
                                  std::string& error) const;
    void rebuildPreview(AniDocument& document, bool startPlaying);
    void startPlayback(AniDocument& document);
    void clearPreviewFrames();
    void showSelectedFrame(AniDocument& document);
    void setSelectedFrame(AniDocument& document, std::size_t index);
    void stopPlayback(AniDocument& document);
    void drawPreview(EditorContext& context, AniDocument& document);
    void drawAnimationCanvas(AniDocument& document, float height);
    void drawCanvasToolbar(float height);
    void pauseForEditing();
    bool acceptFrameDrop(AniDocument& document, std::size_t insertIndex, bool replace);

    ax::Sprite* m_sprite = nullptr;
    std::filesystem::path m_documentPath;
    std::size_t m_selectedFrame = 0;
    bool m_playing              = false;
    bool m_finished             = false;
    bool m_currentLoop          = true;
    float m_frameElapsedMs      = 0.0f;
    std::vector<ax::SpriteFrame*> m_previewFrames;
    std::vector<int> m_frameDelaysMs;
    std::size_t m_seenDocumentRevision = 0;
    std::string m_previewError;

    SceneCanvasViewState m_viewState;
    std::string m_activeToolId = "animation.frame";
    ImVec2 m_dragStartWorld;
    bool m_draggingFrameTransform = false;
    SceneVec2 m_dragStartFrameOffset;
    std::string m_previewVariableKeyEdit;
    std::string m_previewVariableKeyBuffer;
    std::string m_previewVariableError;
};
}  // namespace editor
