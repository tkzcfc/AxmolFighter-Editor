#include "asset_browser/BitmapFontAssetScanner.h"

#include "asset_browser/AssetScanContext.h"
#include "asset_browser/BitmapFontAsset.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

namespace editor
{
namespace
{
std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> parseBitmapFontPages(const std::filesystem::path& fntPath)
{
    std::vector<std::string> pages;
    const std::string content = readTextFile(fntPath);
    if (content.empty())
        return pages;

    static const std::regex pagePattern(R"(page\s+id=\d+\s+file=\"([^\"]+)\")");
    for (std::sregex_iterator it(content.begin(), content.end(), pagePattern), end; it != end; ++it)
        pages.push_back((*it)[1].str());
    return pages;
}
}  // namespace

void BitmapFontAssetScanner::scan(const AssetScanContext& context, std::vector<std::unique_ptr<Asset>>& output) const
{
    for (const std::filesystem::path& file : context.files())
    {
        if (assetLowerExtension(file) != ".fnt" || context.isConsumed(file))
            continue;

        const std::vector<std::string> pages = parseBitmapFontPages(file);
        std::error_code error;
        std::unique_ptr<Asset> font = std::make_unique<BitmapFontAsset>();
        font->absolutePath = file.lexically_normal();
        font->relativePath = context.relativePath(font->absolutePath);
        font->primaryFile = font->relativePath;
        font->name = file.filename().generic_string();
        font->displayName = font->name;
        font->sizeBytes = std::filesystem::file_size(font->absolutePath, error);
        if (error)
            font->sizeBytes = 0;
        bool hasExistingPage = false;
        for (const std::string& page : pages)
        {
            const std::filesystem::path pagePath =
                context.findSibling(std::filesystem::path(page).filename().generic_string());
            if (pagePath.empty())
                continue;

            hasExistingPage = true;
            font->relatedFiles.push_back(context.relativePath(pagePath));
            context.consume(pagePath);
        }

        if (hasExistingPage || !pages.empty())
        {
            context.consume(file);
            output.push_back(std::move(font));
        }
    }
}
}  // namespace editor
