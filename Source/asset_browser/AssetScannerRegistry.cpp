#include "asset_browser/IAssetScanner.h"

#include "asset_browser/BitmapFontAssetScanner.h"
#include "asset_browser/FileAssetScanner.h"
#include "asset_browser/SpineAssetScanner.h"
#include "asset_browser/SpriteAtlasAssetScanner.h"

namespace editor
{
std::vector<std::unique_ptr<IAssetScanner>> createDefaultAssetScanners()
{
    std::vector<std::unique_ptr<IAssetScanner>> scanners;
    scanners.push_back(std::make_unique<SpriteAtlasAssetScanner>());
    scanners.push_back(std::make_unique<BitmapFontAssetScanner>());
    scanners.push_back(std::make_unique<SpineAssetScanner>());
    scanners.push_back(std::make_unique<FileAssetScanner>());
    return scanners;
}
}  // namespace editor
