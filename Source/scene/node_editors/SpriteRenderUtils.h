#pragma once

#include "scene/SceneTypes.h"

#include <string>
#include <vector>

namespace ax
{
class Sprite;
}

namespace editor
{
const std::vector<std::string>& spriteRenderTypeNames();
const std::vector<std::string>& spriteBlendFactorNames();

std::string normalizeSpriteRenderType(const std::string& value);
std::string normalizeSpriteBlendFactor(const std::string& value, const std::string& fallback);
void applySpriteBlendFunc(ax::Sprite& sprite, const std::string& blendSrc, const std::string& blendDst);
void applySpriteRenderSettings(ax::Sprite& sprite, const SpriteNodeData& data);
}  // namespace editor
