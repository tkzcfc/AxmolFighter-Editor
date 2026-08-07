#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class BitmapFontAsset final : public Asset
{
public:
    BitmapFontAsset();

    const char* icon(bool open) const override;
};
}  // namespace editor
