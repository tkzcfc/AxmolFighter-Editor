#pragma once

#include "asset_browser/Asset.h"

#include <memory>
#include <vector>

namespace editor
{
class AssetScanContext;

class IAssetScanner
{
public:
    virtual ~IAssetScanner() = default;
    virtual void scan(const AssetScanContext& context, std::vector<std::unique_ptr<Asset>>& output) const = 0;
};

std::vector<std::unique_ptr<IAssetScanner>> createDefaultAssetScanners();
}  // namespace editor
