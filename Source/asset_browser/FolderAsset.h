#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class FolderAsset final : public Asset
{
public:
    FolderAsset();

    bool isDirectory() const override;
    const char* icon(bool open) const override;
};
}  // namespace editor
