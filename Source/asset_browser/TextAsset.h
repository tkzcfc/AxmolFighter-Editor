#pragma once

#include "asset_browser/Asset.h"

namespace editor
{
class TextAsset final : public Asset
{
public:
    TextAsset();

    bool canOpenDocument() const override;
};
}  // namespace editor
