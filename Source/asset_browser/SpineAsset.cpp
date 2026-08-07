#include "asset_browser/SpineAsset.h"

#include <cstdio>

namespace editor
{
namespace
{
constexpr const char* kIconFilm = "\xef\x80\x88";  // fa-film
}

SpineAsset::SpineAsset() : Asset(AssetKind::Spine) {}

const char* SpineAsset::icon(bool) const
{
    return kIconFilm;
}

void SpineAsset::fillDragPayload(AssetDragPayload& payload) const
{
    Asset::fillDragPayload(payload);
    const std::string atlas = atlasPath.generic_string();
    std::snprintf(payload.atlasPath, sizeof(payload.atlasPath), "%s", atlas.c_str());
}
}  // namespace editor
