#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class LayerAsset final : public Asset
{
public:
    LayerAsset();

    bool canOpenDocument() const override;
};
}  // namespace editor
