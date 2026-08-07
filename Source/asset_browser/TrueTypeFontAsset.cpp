#include "asset_browser/TrueTypeFontAsset.h"

namespace editor
{
namespace
{
constexpr const char* kIconFont = "\xef\x80\xb1";  // fa-font
}

TrueTypeFontAsset::TrueTypeFontAsset() : Asset(AssetKind::TrueTypeFont) {}

const char* TrueTypeFontAsset::icon(bool) const
{
    return kIconFont;
}
}  // namespace editor
