#include "animation/AnimationPreviewController.h"

#include "2d/Node.h"
#include "2d/RenderTexture.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "core/ProjectSettings.h"
#include "documents/AniDocument.h"
#include "documents/BoxDocument.h"
#include "renderer/Renderer.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"
#include "spine/SafeSkeletonAnimation.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace editor
{
namespace
{
int expandedRenderTextureCapacity(int requested, int currentCapacity)
{
    constexpr int kCapacityGranularity = 256;
    if (requested <= currentCapacity)
        return currentCapacity;
    const int grown = currentCapacity > 0 ? static_cast<int>(std::ceil(currentCapacity * 1.5f)) : requested;
    return ((std::max(requested, grown) + kCapacityGranularity - 1) / kCapacityGranularity) * kCapacityGranularity;
}

std::filesystem::path resolvePreviewPath(const std::string& path, const ProjectSettings& settings)
{
    std::filesystem::path resolved(path);
    if (!resolved.is_absolute())
        resolved = settings.resolvedResourceRoot() / resolved;
    return resolved.lexically_normal();
}
}  // namespace

AnimationPreviewController::~AnimationPreviewController()
{
    reset();
}

void AnimationPreviewController::reset()
{
    clearFrames();
    delete m_document;
    m_document = nullptr;
    clearSpine();
    m_kind = BoxPreviewKind::None;
    m_path.clear();
    m_atlasPath.clear();
    m_spineAnimation.clear();
    m_delays.clear();
    m_boundaries.clear();
    m_timeMs = 0;
    m_frameIndex = 0;
    m_spineDurationMs = 0;
    m_spineAnimations.clear();
    m_error.clear();
}

bool AnimationPreviewController::load(const BoxDocument& boxDocument, const ProjectSettings& settings)
{
    const BoxPreviewData& preview = boxDocument.data().preview;
    if (preview.kind == BoxPreviewKind::None)
    {
        reset();
        return false;
    }
    if (preview.kind == BoxPreviewKind::Ani && preview.path.empty())
    {
        reset();
        m_kind = BoxPreviewKind::Ani;
        m_error = "Select an ANI preview asset.";
        return false;
    }
    if (preview.kind == BoxPreviewKind::Spine && (preview.path.empty() || preview.atlasPath.empty()))
    {
        reset();
        m_kind = BoxPreviewKind::Spine;
        m_error = "Select a Spine JSON and atlas preview asset.";
        return false;
    }

    const std::filesystem::path path = resolvePreviewPath(preview.path, settings);
    const std::filesystem::path atlasPath = preview.kind == BoxPreviewKind::Spine
                                                ? resolvePreviewPath(preview.atlasPath, settings)
                                                : std::filesystem::path{};

    // 同资源仅切换 Spine 动画名：不得 reset/销毁 RenderTexture（可能仍有挂起的 begin/end 命令）
    if (preview.kind == BoxPreviewKind::Spine && m_kind == BoxPreviewKind::Spine && path == m_path &&
        atlasPath == m_atlasPath && m_spine != nullptr)
    {
        if (preview.animation == m_spineAnimation)
            return isLoaded();
        return setSpineAnimation(preview.animation);
    }

    if (preview.kind == m_kind && path == m_path && atlasPath == m_atlasPath && preview.animation == m_spineAnimation)
        return isLoaded();

    reset();
    if (preview.kind == BoxPreviewKind::Ani)
        return loadAni(path);
    return loadSpine(path, atlasPath, preview.animation);
}

bool AnimationPreviewController::loadAni(const std::filesystem::path& path)
{
    auto document = std::make_unique<AniDocument>();
    if (!document->open(path))
    {
        m_error = document->lastError();
        return false;
    }
    if (document->animation().frames.empty())
    {
        m_error = "Preview ANI contains no frames.";
        return false;
    }

    for (const AnimationFrameData& frameData : document->animation().frames)
    {
        ax::SpriteFrame* frame = nullptr;
        if (frameData.sourceType == AnimationFrameSourceType::SpriteFrame)
        {
            ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
            if (!cache->isSpriteFramesWithFileLoaded(frameData.atlasPath))
                cache->addSpriteFramesWithFile(frameData.atlasPath);
            frame = cache->getSpriteFrameByName(frameData.frameName);
        }
        else
        {
            std::string imagePath = frameData.sourceType == AnimationFrameSourceType::Template
                                        ? frameData.templatePath
                                        : frameData.path;
            if (frameData.sourceType == AnimationFrameSourceType::Template)
            {
                std::string resolvedTemplate;
                std::string error;
                if (!document->resolveTemplatePath(frameData.templatePath, resolvedTemplate, error))
                {
                    m_error = error;
                    clearFrames();
                    return false;
                }
                imagePath = resolvedTemplate;
            }
            ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(imagePath);
            if (texture)
                frame = ax::SpriteFrame::createWithTexture(
                    texture, ax::Rect(0.0f, 0.0f, static_cast<float>(texture->getPixelsWide()),
                                      static_cast<float>(texture->getPixelsHigh())));
        }
        if (!frame)
        {
            m_error = "Unable to load ANI preview frame.";
            clearFrames();
            return false;
        }
        frame->retain();
        m_frames.push_back(frame);
        m_delays.push_back(std::max(1, frameData.delayMs));
    }

    m_document = document.release();
    m_kind = BoxPreviewKind::Ani;
    m_path = path;
    m_boundaries.push_back(0);
    for (std::int32_t delay : m_delays)
        m_boundaries.push_back(m_boundaries.back() + delay);
    m_error.clear();
    updateFrameFromTime();
    return true;
}

bool AnimationPreviewController::loadSpine(const std::filesystem::path& jsonPath,
                                           const std::filesystem::path& atlasPath,
                                           const std::string& animationName)
{
    m_kind = BoxPreviewKind::Spine;
    m_path = jsonPath;
    m_atlasPath = atlasPath;
    m_spineAnimation = animationName;
    if (jsonPath.empty() || atlasPath.empty())
    {
        m_error = "Select a Spine JSON and atlas preview asset.";
        return false;
    }
    m_spineRoot = ax::Node::create();
    if (!m_spineRoot)
    {
        m_error = "Failed to create Spine preview root.";
        return false;
    }
    m_spineRoot->retain();
    m_spineRoot->onEnter();
    m_spineRoot->onEnterTransitionDidFinish();

    auto [spineError, spineNode] =
        createSafeSkeletonAnimation(jsonPath.generic_string(), atlasPath.generic_string());
    if (!isEditorSpineNode(spineNode))
    {
        m_error = spineError.empty() ? ("Failed to load Spine preview: " + jsonPath.generic_string()) : spineError;
        return false;
    }
    m_spine = spineNode;
    spineSetUpdateOnlyIfVisible(m_spine, false);
    m_spineRoot->addChild(m_spine);

    collectSpineAnimations(m_spine, m_spineAnimations);
    if (m_spineAnimations.empty())
    {
        m_error = "Spine preview contains no animations.";
        return false;
    }
    if (animationName.empty())
    {
        m_error = "Select a Spine preview animation.";
        return false;
    }
    if (!spineHasAnimation(m_spine, animationName))
    {
        m_error = "Spine preview animation was not found: " + animationName;
        return false;
    }

    if (!spineSetAnimation(m_spine, 0, animationName, false))
    {
        m_error = "Failed to select Spine preview animation: " + animationName;
        return false;
    }
    spineKeepCurrentTrackAlive(m_spine, 0);
    m_spineDurationMs = std::max(
        1, static_cast<std::int32_t>(std::lround(spineAnimationDuration(m_spine, animationName) * 1000.0f)));
    m_error.clear();
    setTime(0);
    return true;
}

bool AnimationPreviewController::setSpineAnimation(const std::string& animationName)
{
    if (!m_spine || !isSpine())
    {
        m_error = "Spine preview is not loaded.";
        return false;
    }
    if (animationName.empty())
    {
        m_error = "Select a Spine preview animation.";
        return false;
    }
    if (!spineHasAnimation(m_spine, animationName))
    {
        m_error = "Spine preview animation was not found: " + animationName;
        return false;
    }

    if (!spineSetAnimation(m_spine, 0, animationName, false))
    {
        m_error = "Failed to select Spine preview animation: " + animationName;
        return false;
    }
    spineKeepCurrentTrackAlive(m_spine, 0);
    m_spineAnimation  = animationName;
    m_spineDurationMs = std::max(
        1, static_cast<std::int32_t>(std::lround(spineAnimationDuration(m_spine, animationName) * 1000.0f)));
    m_error.clear();
    setTime(0);
    return true;
}

void AnimationPreviewController::setTime(std::int32_t timeMs)
{
    m_timeMs = std::max(0, timeMs);
    if (isAni())
    {
        m_timeMs = std::min(m_timeMs, std::max(0, durationMs() - 1));
        updateFrameFromTime();
        return;
    }
    if (!m_spine || !isSpine())
        return;

    if (m_timeMs > m_spineDurationMs)
    {
        m_spine->setVisible(false);
        return;
    }
    m_spine->setVisible(true);
    spineSeekCurrentTrack(m_spine, 0, static_cast<float>(m_timeMs) / 1000.0f);
    spineUpdate(m_spine, 0.0f);
}

bool AnimationPreviewController::isLoaded() const
{
    return isAni() ? m_document != nullptr && !m_frames.empty() : isSpine() && m_spine != nullptr && m_spineDurationMs > 0;
}

bool AnimationPreviewController::isAni() const { return m_kind == BoxPreviewKind::Ani; }
bool AnimationPreviewController::isSpine() const { return m_kind == BoxPreviewKind::Spine; }
std::int32_t AnimationPreviewController::timeMs() const { return m_timeMs; }
std::int32_t AnimationPreviewController::durationMs() const
{
    if (isSpine())
        return m_spineDurationMs;
    return m_boundaries.empty() ? 0 : m_boundaries.back();
}
std::size_t AnimationPreviewController::frameIndex() const { return m_frameIndex; }
const std::vector<std::int32_t>& AnimationPreviewController::frameBoundaries() const { return m_boundaries; }
ax::SpriteFrame* AnimationPreviewController::currentFrame() const
{
    return m_frameIndex < m_frames.size() ? m_frames[m_frameIndex] : nullptr;
}
const AnimationFrameData* AnimationPreviewController::currentFrameData() const
{
    if (!m_document || m_frameIndex >= m_document->animation().frames.size())
        return nullptr;
    return &m_document->animation().frames[m_frameIndex];
}
const std::vector<std::string>& AnimationPreviewController::spineAnimations() const { return m_spineAnimations; }

void AnimationPreviewController::renderSpine(const SceneCanvasFrame& frame)
{
    if (!m_spineRoot || !m_spine)
        return;

    m_spineRenderTextureRequestedWidth = std::max(1, static_cast<int>(std::ceil(frame.canvasSize.x)));
    m_spineRenderTextureRequestedHeight = std::max(1, static_cast<int>(std::ceil(frame.canvasSize.y)));
    if (!m_spineRenderTexture || m_spineRenderTextureRequestedWidth > m_spineRenderTextureCapacityWidth ||
        m_spineRenderTextureRequestedHeight > m_spineRenderTextureCapacityHeight)
    {
        if (m_spineRenderTexture)
        {
            if (ax::Director* director = ax::Director::getInstance())
            {
                if (ax::Renderer* renderer = director->getRenderer())
                    renderer->clean();
            }
            m_spineRenderTexture->release();
            m_spineRenderTexture = nullptr;
        }
        m_spineRenderTextureCapacityWidth =
            expandedRenderTextureCapacity(m_spineRenderTextureRequestedWidth, m_spineRenderTextureCapacityWidth);
        m_spineRenderTextureCapacityHeight =
            expandedRenderTextureCapacity(m_spineRenderTextureRequestedHeight, m_spineRenderTextureCapacityHeight);
        m_spineRenderTexture = ax::RenderTexture::create(m_spineRenderTextureCapacityWidth, m_spineRenderTextureCapacityHeight,
                                                          ax::backend::PixelFormat::RGBA8, false);
        if (m_spineRenderTexture)
            m_spineRenderTexture->retain();
        else
            m_spineRenderTextureCapacityWidth = m_spineRenderTextureCapacityHeight = 0;
    }
    if (!m_spineRenderTexture)
        return;

    m_spineRoot->setContentSize(ax::Size(frame.canvasSize.x, frame.canvasSize.y));
    m_spine->setPosition(ax::Vec2(frame.canvasSize.x * 0.5f + frame.pan.x, frame.canvasSize.y * 0.5f - frame.pan.y));
    m_spine->setScale(frame.zoom);
    m_spineRenderTexture->beginWithClear(0.0f, 0.0f, 0.0f, 0.0f);
    m_spineRoot->visit();
    m_spineRenderTexture->end();
}

ax::Sprite* AnimationPreviewController::spineRenderSprite() const
{
    return m_spineRenderTexture ? m_spineRenderTexture->getSprite() : nullptr;
}
int AnimationPreviewController::spineRenderTextureRequestedWidth() const { return m_spineRenderTextureRequestedWidth; }
int AnimationPreviewController::spineRenderTextureRequestedHeight() const { return m_spineRenderTextureRequestedHeight; }
int AnimationPreviewController::spineRenderTextureCapacityWidth() const { return m_spineRenderTextureCapacityWidth; }
int AnimationPreviewController::spineRenderTextureCapacityHeight() const { return m_spineRenderTextureCapacityHeight; }
const std::string& AnimationPreviewController::error() const { return m_error; }

void AnimationPreviewController::clearFrames()
{
    for (ax::SpriteFrame* frame : m_frames)
        frame->release();
    m_frames.clear();
}

void AnimationPreviewController::clearSpine()
{
    // beginWithClear/end 会把命令挂到 Renderer 队列；若先释放 RT，Scene::render 时 onBegin 会读空悬指针
    if (m_spineRenderTexture)
    {
        if (ax::Director* director = ax::Director::getInstance())
        {
            if (ax::Renderer* renderer = director->getRenderer())
                renderer->clean();
        }
        m_spineRenderTexture->release();
        m_spineRenderTexture = nullptr;
    }
    m_spineRenderTextureRequestedWidth = 0;
    m_spineRenderTextureRequestedHeight = 0;
    m_spineRenderTextureCapacityWidth = 0;
    m_spineRenderTextureCapacityHeight = 0;
    if (m_spineRoot)
    {
        if (m_spineRoot->isRunning())
        {
            m_spineRoot->onExitTransitionDidStart();
            m_spineRoot->onExit();
        }
        m_spineRoot->removeAllChildrenWithCleanup(true);
        m_spineRoot->release();
    }
    m_spineRoot = nullptr;
    m_spine = nullptr;
}

void AnimationPreviewController::updateFrameFromTime()
{
    if (m_boundaries.size() < 2)
    {
        m_frameIndex = 0;
        return;
    }
    const auto it = std::upper_bound(m_boundaries.begin(), m_boundaries.end(), m_timeMs);
    m_frameIndex = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, (it - m_boundaries.begin()) - 1));
    m_frameIndex = std::min(m_frameIndex, m_frames.size() - 1);
}
}  // namespace editor
