#pragma once

#include "asset_browser/AssetKind.h"

#include <cstdint>
#include <cstddef>

namespace editor
{
inline constexpr const char* kAssetDragPayloadType       = "ASSET_BROWSER_ITEM";
inline constexpr const char* kAssetDragBatchPayloadType  = "ASSET_BROWSER_ITEMS";
inline constexpr std::size_t kMaxAssetDragBatchItems     = 64;
inline constexpr std::size_t kAssetDragPathCapacity      = 260;
inline constexpr std::size_t kAssetDragExtensionCapacity = 32;
inline constexpr std::size_t kAssetDragFrameNameCapacity = 260;

struct AssetDragPayload
{
    AssetKind kind                              = AssetKind::Unknown;
    char relativePath[kAssetDragPathCapacity]   = {};
    char extension[kAssetDragExtensionCapacity] = {};
    char atlasPath[kAssetDragPathCapacity]      = {};
    char frameName[kAssetDragFrameNameCapacity] = {};
    int width                                   = 0;
    int height                                  = 0;
    int sourceWidth                             = 0;
    int sourceHeight                            = 0;
};

struct AssetDragPayloadBatch
{
    std::uint32_t count = 0;
    AssetDragPayload items[kMaxAssetDragBatchItems];
};
}  // namespace editor
