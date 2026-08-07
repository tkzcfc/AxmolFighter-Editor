#include "scene/node_editors/SpriteSliceUtils.h"

#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"

#include <algorithm>

namespace editor
{
namespace
{
constexpr float kMinCenterSize = 0.001f;

float safeDimension(float value)
{
    return std::max(1.0f, value);
}

SpriteSliceMargins clampNormalizedMargins(SpriteSliceMargins margins)
{
    margins.left = std::clamp(margins.left, 0.0f, 1.0f - kMinCenterSize);
    margins.right = std::clamp(margins.right, 0.0f, 1.0f - kMinCenterSize);
    if (margins.left + margins.right > 1.0f - kMinCenterSize)
    {
        const float scale = (1.0f - kMinCenterSize) / (margins.left + margins.right);
        margins.left *= scale;
        margins.right *= scale;
    }

    margins.top = std::clamp(margins.top, 0.0f, 1.0f - kMinCenterSize);
    margins.bottom = std::clamp(margins.bottom, 0.0f, 1.0f - kMinCenterSize);
    if (margins.top + margins.bottom > 1.0f - kMinCenterSize)
    {
        const float scale = (1.0f - kMinCenterSize) / (margins.top + margins.bottom);
        margins.top *= scale;
        margins.bottom *= scale;
    }
    return margins;
}
}  // namespace

SceneSize spriteSliceReferenceSize(const SceneNode& node)
{
    if (node.sprite.sourceType == "SpriteFrame")
    {
        if (!node.sprite.atlasPath.empty() && !node.sprite.frameName.empty())
        {
            ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
            if (!cache->isSpriteFramesWithFileLoaded(node.sprite.atlasPath))
                cache->addSpriteFramesWithFile(node.sprite.atlasPath);

            if (ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.sprite.frameName))
            {
                const ax::Vec2 size = frame->getOriginalSize();
                if (size.x > 0.0f && size.y > 0.0f)
                    return {size.x, size.y};
            }
        }
    }
    else if (!node.sprite.imagePath.empty())
    {
        if (ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(node.sprite.imagePath))
        {
            if (texture->getPixelsWide() > 0 && texture->getPixelsHigh() > 0)
            {
                return {static_cast<float>(texture->getPixelsWide()),
                        static_cast<float>(texture->getPixelsHigh())};
            }
        }
    }

    return {safeDimension(node.size.width), safeDimension(node.size.height)};
}

bool spriteSliceHasSourceReferenceSize(const SceneNode& node)
{
    if (node.sprite.sourceType == "SpriteFrame")
    {
        if (node.sprite.atlasPath.empty() || node.sprite.frameName.empty())
            return false;

        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.sprite.atlasPath))
            cache->addSpriteFramesWithFile(node.sprite.atlasPath);
        if (ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.sprite.frameName))
        {
            const ax::Vec2 size = frame->getOriginalSize();
            return size.x > 0.0f && size.y > 0.0f;
        }
        return false;
    }

    if (node.sprite.imagePath.empty())
        return false;

    ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(node.sprite.imagePath);
    return texture && texture->getPixelsWide() > 0 && texture->getPixelsHigh() > 0;
}

SpriteSliceMargins spriteSliceNormalizedMargins(const SpriteNodeData& sprite)
{
    return clampNormalizedMargins({sprite.sliceCenterOrigin.x,
                                   1.0f - sprite.sliceCenterOrigin.x - sprite.sliceCenterSize.x,
                                   sprite.sliceCenterOrigin.y,
                                   1.0f - sprite.sliceCenterOrigin.y - sprite.sliceCenterSize.y});
}

SpriteSliceMargins spriteSlicePixelMargins(const SceneNode& node)
{
    const SceneSize referenceSize = spriteSliceReferenceSize(node);
    const SpriteSliceMargins normalized = spriteSliceNormalizedMargins(node.sprite);
    return {normalized.left * referenceSize.width,
            normalized.right * referenceSize.width,
            normalized.top * referenceSize.height,
            normalized.bottom * referenceSize.height};
}

void setSpriteSliceNormalizedMargins(SpriteNodeData& sprite, SpriteSliceMargins margins)
{
    margins = clampNormalizedMargins(margins);
    sprite.sliceCenterOrigin = {margins.left, margins.top};
    sprite.sliceCenterSize = {1.0f - margins.left - margins.right, 1.0f - margins.top - margins.bottom};
}

void setSpriteSlicePixelMargins(SceneNode& node, SpriteSliceMargins margins)
{
    const SceneSize referenceSize = spriteSliceReferenceSize(node);
    setSpriteSliceNormalizedMargins(node.sprite,
                                    {margins.left / safeDimension(referenceSize.width),
                                     margins.right / safeDimension(referenceSize.width),
                                     margins.top / safeDimension(referenceSize.height),
                                     margins.bottom / safeDimension(referenceSize.height)});
}
}  // namespace editor
