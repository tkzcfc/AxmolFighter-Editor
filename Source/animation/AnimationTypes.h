#pragma once

#include "scene/SceneTypes.h"

#include <map>
#include <string>
#include <vector>

namespace editor
{
enum class AnimationFrameSourceType
{
    Texture,
    SpriteFrame,
    Template
};

using AnimationPreviewVariables = std::map<std::string, std::string>;

struct AnimationFrameData
{
    AnimationFrameSourceType sourceType = AnimationFrameSourceType::Texture;
    std::string path;
    std::string atlasPath;
    std::string frameName;
    std::string templatePath;
    int delayMs = 33;
    SceneVec2 offset;
    // Normalized sprite anchor in Cartesian space (0,0)=bottom-left, (0.5,0.5)=center, (0,1)=top-left.
    SceneVec2 anchor = {0.5f, 0.5f};
    SceneVec2 scale = {1.0f, 1.0f};
    float rotation  = 0.0f;
    SceneColor color;
    bool operator==(const AnimationFrameData&) const = default;
};

struct FrameAnimationData
{
    static constexpr int kCurrentVersion = 1;

    int version = kCurrentVersion;
    bool loop   = true;
    AnimationPreviewVariables previewVars;
    std::vector<AnimationFrameData> frames;

    bool operator==(const FrameAnimationData&) const = default;
};
}  // namespace editor
