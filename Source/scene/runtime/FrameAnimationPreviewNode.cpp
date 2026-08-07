#include "scene/runtime/FrameAnimationPreviewNode.h"

#include "2d/Sprite.h"
#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "documents/AniDocument.h"
#include "platform/FileUtils.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"
#include "scene/node_editors/SpriteRenderUtils.h"

#include <algorithm>
#include <utility>

namespace editor
{
namespace
{
uint8_t colorByte(float value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

}  // namespace

FrameAnimationPreviewNode::~FrameAnimationPreviewNode()
{
    clearFrames();
}

FrameAnimationPreviewNode* FrameAnimationPreviewNode::create(const std::string& aniPath,
                                                             const AnimationPreviewVariables& variables,
                                                             bool loop,
                                                             bool playing,
                                                             const std::string& blendSrc,
                                                             const std::string& blendDst,
                                                             std::string& error)
{
    auto* result = new (std::nothrow) FrameAnimationPreviewNode();
    if (!result || !result->init() ||
        !result->configure(aniPath, variables, loop, playing, blendSrc, blendDst, error))
    {
        if (result)
            result->release();
        return nullptr;
    }
    result->autorelease();
    return result;
}

bool FrameAnimationPreviewNode::init()
{
    if (!ax::Node::init())
        return false;

    m_sprite = ax::Sprite::create();
    if (!m_sprite)
        return false;
    addChild(m_sprite);
    scheduleUpdate();
    return true;
}

bool FrameAnimationPreviewNode::configure(const std::string& aniPath,
                                          const AnimationPreviewVariables& variables,
                                          bool loop,
                                          bool playing,
                                          const std::string& blendSrc,
                                          const std::string& blendDst,
                                          std::string& error)
{
    error.clear();
    if (aniPath.empty())
    {
        error = "Frame animation ANI path is empty.";
        return false;
    }

    if (!m_aniPath.empty() && m_aniPath == aniPath && m_variables == variables)
    {
        m_loop    = loop;
        m_playing = playing;
        m_blendSrc = blendSrc;
        m_blendDst = blendDst;
        applyCurrentFrame();
        return true;
    }

    ax::FileUtils* fileUtils          = ax::FileUtils::getInstance();
    const std::string resolvedAniPath = fileUtils->fullPathForFilename(aniPath);
    if (resolvedAniPath.empty() || !fileUtils->isFileExist(resolvedAniPath))
    {
        error = "Frame animation ANI file was not found: " + aniPath;
        return false;
    }

    AniDocument document;
    if (!document.open(resolvedAniPath))
    {
        error = document.lastError();
        return false;
    }

    AnimationPreviewVariables mergedVariables = document.animation().previewVars;
    for (const auto& [key, value] : variables)
        mergedVariables[key] = value;

    std::vector<PreviewFrame> loadedFrames;
    loadedFrames.reserve(document.animation().frames.size());
    const auto releaseLoadedFrames = [&loadedFrames]() {
        for (auto& frame : loadedFrames)
        {
            if (frame.spriteFrame)
                frame.spriteFrame->release();
        }
        loadedFrames.clear();
    };
    for (const AnimationFrameData& frameData : document.animation().frames)
    {
        std::string imagePath;
        if (frameData.sourceType == AnimationFrameSourceType::Template)
        {
            if (!document.resolveTemplatePath(frameData.templatePath, mergedVariables, imagePath, error))
            {
                releaseLoadedFrames();
                return false;
            }
        }
        else if (frameData.sourceType == AnimationFrameSourceType::SpriteFrame)
        {
            imagePath = frameData.atlasPath;
        }
        else
        {
            imagePath = frameData.path;
        }

        ax::SpriteFrame* spriteFrame = nullptr;
        if (frameData.sourceType == AnimationFrameSourceType::SpriteFrame)
        {
            const std::string atlasPath = fileUtils->fullPathForFilename(imagePath);
            if (atlasPath.empty() || !fileUtils->isFileExist(atlasPath))
            {
                error = "Frame animation atlas was not found: " + imagePath;
                releaseLoadedFrames();
                return false;
            }
            auto* cache = ax::SpriteFrameCache::getInstance();
            if (!cache->isSpriteFramesWithFileLoaded(atlasPath))
                cache->addSpriteFramesWithFile(atlasPath);
            spriteFrame = cache->getSpriteFrameByName(frameData.frameName);
        }
        else
        {
            const std::string texturePath = fileUtils->fullPathForFilename(imagePath);
            ax::Texture2D* texture        = fileUtils->isFileExist(texturePath)
                                                ? ax::Director::getInstance()->getTextureCache()->addImage(texturePath)
                                                : nullptr;
            if (texture)
            {
                spriteFrame = ax::SpriteFrame::createWithTexture(
                    texture, ax::Rect(0.0f, 0.0f, static_cast<float>(texture->getPixelsWide()),
                                      static_cast<float>(texture->getPixelsHigh())));
            }
        }

        if (!spriteFrame)
        {
            error = "Failed to load frame animation image: " + imagePath;
            releaseLoadedFrames();
            return false;
        }

        spriteFrame->retain();
        loadedFrames.push_back({spriteFrame, frameData});
    }

    clearFrames();
    m_frames       = std::move(loadedFrames);
    m_aniPath      = aniPath;
    m_variables    = variables;
    m_blendSrc     = blendSrc;
    m_blendDst     = blendDst;
    m_loop         = loop;
    m_playing      = playing;
    m_currentFrame = 0;
    m_elapsedMs    = 0.0;
    applyCurrentFrame();
    return true;
}

void FrameAnimationPreviewNode::update(float delta)
{
    ax::Node::update(delta);
    if (!m_playing || m_frames.empty() || delta <= 0.0f)
        return;

    m_elapsedMs += static_cast<double>(delta) * 1000.0;
    while (m_playing && !m_frames.empty())
    {
        const int delayMs = std::max(1, m_frames[m_currentFrame].data.delayMs);
        if (m_elapsedMs < delayMs)
            break;

        m_elapsedMs -= delayMs;
        if (m_currentFrame + 1 < m_frames.size())
        {
            ++m_currentFrame;
            applyCurrentFrame();
        }
        else if (m_loop)
        {
            m_currentFrame = 0;
            applyCurrentFrame();
        }
        else
        {
            m_playing   = false;
            m_elapsedMs = 0.0;
        }
    }
}

void FrameAnimationPreviewNode::clearFrames()
{
    for (auto& frame : m_frames)
    {
        if (frame.spriteFrame)
            frame.spriteFrame->release();
    }
    m_frames.clear();
}

void FrameAnimationPreviewNode::applyCurrentFrame()
{
    if (!m_sprite || m_frames.empty() || m_currentFrame >= m_frames.size())
        return;

    const PreviewFrame& frame = m_frames[m_currentFrame];
    m_sprite->setSpriteFrame(frame.spriteFrame);
    applySpriteBlendFunc(*m_sprite, m_blendSrc, m_blendDst);
    m_sprite->setAnchorPoint(ax::Vec2(frame.data.anchor.x, frame.data.anchor.y));
    m_sprite->setPosition(frame.data.offset.x, frame.data.offset.y);
    m_sprite->setScale(frame.data.scale.x, frame.data.scale.y);
    m_sprite->setRotation(frame.data.rotation);
    m_sprite->setColor({colorByte(frame.data.color.r), colorByte(frame.data.color.g), colorByte(frame.data.color.b)});
    m_sprite->setOpacity(colorByte(frame.data.color.a));
}
}  // namespace editor
