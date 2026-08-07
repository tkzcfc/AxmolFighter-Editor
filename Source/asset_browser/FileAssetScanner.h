#pragma once

#include "asset_browser/IAssetScanner.h"

namespace editor
{
class FileAssetScanner final : public IAssetScanner
{
public:
    void scan(const AssetScanContext& context, std::vector<std::unique_ptr<Asset>>& output) const override;
};
}  // namespace editor
