#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class TrueTypeFontAsset final : public Asset
{
public:
    TrueTypeFontAsset();

    const char* icon(bool open) const override;
};
}  // namespace editor
