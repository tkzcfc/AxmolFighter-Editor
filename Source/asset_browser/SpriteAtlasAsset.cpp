#include "asset_browser/SpriteAtlasAsset.h"

namespace editor
{
namespace
{
constexpr const char* kIconFolderOpen = "\xef\x81\xbc";  // fa-folder-open
constexpr const char* kIconArchive    = "\xef\x86\x87";  // fa-file-archive
}

SpriteAtlasAsset::SpriteAtlasAsset() : Asset(AssetKind::SpriteAtlas) {}

const char* SpriteAtlasAsset::icon(bool open) const
{
    return open ? kIconFolderOpen : kIconArchive;
}
}  // namespace editor
