#pragma once

#include "animation/AnimationTypes.h"
#include "combat/CombatBoxTypes.h"
#include "scene/canvas/SceneCanvas.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ax
{
class SpriteFrame;
class Sprite;
class RenderTexture;
class Node;
}

namespace editor
{
class AniDocument;
class BoxDocument;
class ProjectSettings;

class AnimationPreviewController
{
public:
    AnimationPreviewController() = default;
    ~AnimationPreviewController();

    void reset();
    bool load(const BoxDocument& boxDocument, const ProjectSettings& settings);
    void setTime(std::int32_t timeMs);

    bool isLoaded() const;
    bool isAni() const;
    bool isSpine() const;
    std::int32_t timeMs() const;
    std::int32_t durationMs() const;
    std::size_t frameIndex() const;
    const std::vector<std::int32_t>& frameBoundaries() const;
    ax::SpriteFrame* currentFrame() const;
    const AnimationFrameData* currentFrameData() const;
    const std::vector<std::string>& spineAnimations() const;
    void renderSpine(const SceneCanvasFrame& frame);
    ax::Sprite* spineRenderSprite() const;
    int spineRenderTextureRequestedWidth() const;
    int spineRenderTextureRequestedHeight() const;
    int spineRenderTextureCapacityWidth() const;
    int spineRenderTextureCapacityHeight() const;
    const std::string& error() const;

private:
    bool loadAni(const std::filesystem::path& path);
    bool loadSpine(const std::filesystem::path& jsonPath,
                   const std::filesystem::path& atlasPath,
                   const std::string& animationName);
    /** 同骨骼资源下切换动画，不销毁 RenderTexture / Skeleton */
    bool setSpineAnimation(const std::string& animationName);
    void clearFrames();
    void clearSpine();
    void updateFrameFromTime();

    BoxPreviewKind m_kind = BoxPreviewKind::None;
    std::filesystem::path m_path;
    std::filesystem::path m_atlasPath;
    std::string m_spineAnimation;
    AniDocument* m_document = nullptr;
    std::vector<ax::SpriteFrame*> m_frames;
    std::vector<std::int32_t> m_delays;
    std::vector<std::int32_t> m_boundaries;
    std::int32_t m_timeMs = 0;
    std::size_t m_frameIndex = 0;
    ax::Node* m_spine = nullptr;
    ax::Node* m_spineRoot = nullptr;
    ax::RenderTexture* m_spineRenderTexture = nullptr;
    int m_spineRenderTextureRequestedWidth = 0;
    int m_spineRenderTextureRequestedHeight = 0;
    int m_spineRenderTextureCapacityWidth = 0;
    int m_spineRenderTextureCapacityHeight = 0;
    std::int32_t m_spineDurationMs = 0;
    std::vector<std::string> m_spineAnimations;
    std::string m_error;
};
}  // namespace editor
