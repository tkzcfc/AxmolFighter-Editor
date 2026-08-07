#include "asset_browser/SpineAssetScanner.h"

#include "asset_browser/AssetScanContext.h"
#include "asset_browser/SpineAsset.h"

#include <system_error>

namespace editor
{
void SpineAssetScanner::scan(const AssetScanContext& context, std::vector<std::unique_ptr<Asset>>& output) const
{
    for (const std::filesystem::path& file : context.files())
    {
        if (assetLowerExtension(file) != ".json" || context.isConsumed(file))
            continue;

        const std::filesystem::path atlasPath = context.findSibling(file.stem().generic_string() + ".atlas");
        const std::filesystem::path texturePath = context.findSibling(file.stem().generic_string() + ".png");
        if (atlasPath.empty() || texturePath.empty())
            continue;

        std::error_code error;
        auto spine = std::make_unique<SpineAsset>();
        spine->absolutePath = file.lexically_normal();
        spine->relativePath = context.relativePath(spine->absolutePath);
        spine->primaryFile = spine->relativePath;
        spine->name = file.filename().generic_string();
        spine->displayName = spine->name;
        spine->atlasPath = context.relativePath(atlasPath);
        spine->sizeBytes = std::filesystem::file_size(spine->absolutePath, error);
        if (error)
            spine->sizeBytes = 0;
        spine->relatedFiles.push_back(context.relativePath(atlasPath));
        spine->relatedFiles.push_back(context.relativePath(texturePath));
        context.consume(file);
        context.consume(atlasPath);
        context.consume(texturePath);
        output.push_back(std::move(spine));
    }
}
}  // namespace editor
