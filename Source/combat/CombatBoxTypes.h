#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace editor
{
struct Vec3i
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    bool operator==(const Vec3i&) const = default;
};

struct CombatBox
{
    Vec3i pos;
    Vec3i size;

    bool operator==(const CombatBox&) const = default;
};

struct BoxKey
{
    std::int32_t timeMs = 0;
    std::optional<CombatBox> box;

    bool operator==(const BoxKey&) const = default;
};

struct BoxTrack
{
    std::string name;
    // "attack" | "damage" | "hitbox"（hitbox=黑月式中性几何，不分攻受）
    std::string kind = "hitbox";
    std::vector<BoxKey> keys;

    bool operator==(const BoxTrack&) const = default;
};

inline const BoxKey* resolveBoxKeyAt(const BoxTrack& track, std::int32_t timeMs)
{
    const BoxKey* result = nullptr;
    for (const BoxKey& key : track.keys)
    {
        if (key.timeMs > timeMs)
            break;
        result = &key;
    }
    return result;
}

inline BoxKey* resolveBoxKeyAt(BoxTrack& track, std::int32_t timeMs)
{
    return const_cast<BoxKey*>(resolveBoxKeyAt(static_cast<const BoxTrack&>(track), timeMs));
}

inline const CombatBox* resolveBoxAt(const BoxTrack& track, std::int32_t timeMs)
{
    const BoxKey* key = resolveBoxKeyAt(track, timeMs);
    return key && key->box ? &*key->box : nullptr;
}

inline CombatBox* resolveBoxAt(BoxTrack& track, std::int32_t timeMs)
{
    BoxKey* key = resolveBoxKeyAt(track, timeMs);
    return key && key->box ? &*key->box : nullptr;
}

struct BoxEvent
{
    std::int32_t timeMs = 0;
    std::string type;
    std::string value;

    bool operator==(const BoxEvent&) const = default;
};

enum class BoxPreviewKind
{
    None,
    Ani,
    Spine
};

struct BoxPreviewData
{
    BoxPreviewKind kind = BoxPreviewKind::None;
    std::string path;
    std::string atlasPath;
    std::string animation;

    bool operator==(const BoxPreviewData&) const = default;
};

struct BoxData
{
    static constexpr std::int32_t kCurrentVersion = 1;

    std::int32_t version = kCurrentVersion;
    std::int32_t duration = 1000;
    BoxPreviewData preview;
    std::vector<BoxTrack> tracks;
    std::vector<BoxEvent> events;

    bool operator==(const BoxData&) const = default;
};
}  // namespace editor
