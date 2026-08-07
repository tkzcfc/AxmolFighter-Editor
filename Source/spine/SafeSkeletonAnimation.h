#pragma once

#include <string>
#include <tuple>

namespace spine
{
class SkeletonAnimation;
}

namespace editor
{
// Loads Spine via SkeletonJson + createWithData, avoiding createWithJsonFile crash when
// skeletonData is null (missing/invalid files). On success the returned node is autoreleased
// and owns skeletonData + atlas. On failure returns {error, nullptr}.
std::tuple<std::string, spine::SkeletonAnimation*> createSafeSkeletonAnimation(
    const std::string& jsonPath,
    const std::string& atlasPath,
    float scale = 1.0f);
}  // namespace editor
