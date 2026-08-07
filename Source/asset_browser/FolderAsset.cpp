#include "asset_browser/FolderAsset.h"

namespace editor
{
namespace
{
constexpr const char* kIconFolderClosed = "\xef\x81\xbb";  // fa-folder
constexpr const char* kIconFolderOpen   = "\xef\x81\xbc";  // fa-folder-open
}

FolderAsset::FolderAsset() : Asset(AssetKind::Folder) {}

bool FolderAsset::isDirectory() const
{
    return true;
}

const char* FolderAsset::icon(bool open) const
{
    return open ? kIconFolderOpen : kIconFolderClosed;
}
}  // namespace editor
