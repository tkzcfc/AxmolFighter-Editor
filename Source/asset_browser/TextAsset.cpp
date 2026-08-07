#include "asset_browser/TextAsset.h"

namespace editor
{
TextAsset::TextAsset() : Asset(AssetKind::Text) {}

bool TextAsset::canOpenDocument() const
{
    return true;
}
}  // namespace editor
