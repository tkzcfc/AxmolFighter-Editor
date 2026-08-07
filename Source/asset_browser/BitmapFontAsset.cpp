#include "asset_browser/BitmapFontAsset.h"

namespace editor
{
namespace
{
constexpr const char* kIconFont = "\xef\x80\xb1";  // fa-font
}

BitmapFontAsset::BitmapFontAsset() : Asset(AssetKind::BitmapFont) {}

const char* BitmapFontAsset::icon(bool) const
{
    return kIconFont;
}
}  // namespace editor
