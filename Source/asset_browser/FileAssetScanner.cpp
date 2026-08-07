#include "asset_browser/FileAssetScanner.h"

#include "asset_browser/AssetScanContext.h"

#include <algorithm>
#include <cctype>

namespace editor
{
namespace
{
bool isHiddenAssetExtension(const std::filesystem::path& file)
{
    std::string extension = file.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".vf";
}
}  // namespace

void FileAssetScanner::scan(const AssetScanContext& context, std::vector<std::unique_ptr<Asset>>& output) const
{
    for (const std::filesystem::path& file : context.files())
    {
        if (context.isConsumed(file) || isHiddenAssetExtension(file))
            continue;
        output.push_back(context.createFileAsset(file));
    }
}
}  // namespace editor
