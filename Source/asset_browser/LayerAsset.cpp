#include "asset_browser/LayerAsset.h"

namespace editor
{
LayerAsset::LayerAsset() : Asset(AssetKind::Layer) {}

bool LayerAsset::canOpenDocument() const
{
    return true;
}
}  // namespace editor
