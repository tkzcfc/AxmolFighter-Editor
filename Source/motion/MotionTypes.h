#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace editor
{
enum class MotionAnimationType : std::int8_t
{
    Ani   = 0,
    Spine = 1,
};

struct MotionAnimationEntry
{
    std::string id;
    MotionAnimationType type = MotionAnimationType::Ani;
    /** type=Ani: relative .ani path/filename; type=Spine: spine animation name */
    std::string source;
    std::string box;
    std::string tag;

    bool operator==(const MotionAnimationEntry&) const = default;
};

struct MotionEntry
{
    std::string name;
    std::vector<MotionAnimationEntry> animations;

    bool operator==(const MotionEntry&) const = default;
};

struct MotionData
{
    static constexpr std::int32_t kCurrentVersion = 1;

    std::int32_t version = kCurrentVersion;
    std::vector<MotionEntry> motions;

    bool operator==(const MotionData&) const = default;
};
}  // namespace editor
