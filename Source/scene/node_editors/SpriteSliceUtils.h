#pragma once

#include "scene/SceneTypes.h"

namespace editor
{
struct SpriteSliceMargins
{
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
};

SceneSize spriteSliceReferenceSize(const SceneNode& node);
bool spriteSliceHasSourceReferenceSize(const SceneNode& node);
SpriteSliceMargins spriteSliceNormalizedMargins(const SpriteNodeData& sprite);
SpriteSliceMargins spriteSlicePixelMargins(const SceneNode& node);
void setSpriteSliceNormalizedMargins(SpriteNodeData& sprite, SpriteSliceMargins margins);
void setSpriteSlicePixelMargins(SceneNode& node, SpriteSliceMargins margins);
}  // namespace editor
