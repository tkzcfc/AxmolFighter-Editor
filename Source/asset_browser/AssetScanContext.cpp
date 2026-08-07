#include "asset_browser/AssetScanContext.h"

#include "asset_browser/AssetFileTypeRegistry.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace editor
{
std::string assetLowerString(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string assetLowerExtension(const std::filesystem::path& path)
{
    return assetLowerString(path.extension().generic_string());
}

std::string assetLowerFilename(const std::filesystem::path& path)
{
    return assetLowerString(path.filename().generic_string());
}

std::string assetNormalizedPathKey(const std::filesystem::path& path)
{
    return assetLowerString(path.lexically_normal().generic_string());
}

std::filesystem::path assetRelativePath(const std::filesystem::path& root, const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::relative(path.lexically_normal(), root, error).lexically_normal();
}

bool sortAssets(const std::unique_ptr<Asset>& lhs, const std::unique_ptr<Asset>& rhs)
{
    const bool lhsDirectory  = lhs && lhs->isDirectory();
    const bool rhsDirectory  = rhs && rhs->isDirectory();
    const bool lhsExpandable = lhs && (lhsDirectory || !lhs->children.empty());
    const bool rhsExpandable = rhs && (rhsDirectory || !rhs->children.empty());
    if (lhsDirectory != rhsDirectory)
        return lhsDirectory;
    if (lhsExpandable != rhsExpandable)
        return lhsExpandable;
    const std::string lhsName = lhs ? (lhs->displayName.empty() ? lhs->name : lhs->displayName) : "";
    const std::string rhsName = rhs ? (rhs->displayName.empty() ? rhs->name : rhs->displayName) : "";
    return assetLowerString(lhsName) < assetLowerString(rhsName);
}

AssetScanContext::AssetScanContext(const std::filesystem::path& root,
                                   const std::vector<std::filesystem::path>& files,
                                   const std::unordered_map<std::string, std::filesystem::path>& filesByName,
                                   std::set<std::string>& consumed)
    : m_root(root), m_files(files), m_filesByName(filesByName), m_consumed(consumed)
{}

const std::filesystem::path& AssetScanContext::root() const
{
    return m_root;
}

const std::vector<std::filesystem::path>& AssetScanContext::files() const
{
    return m_files;
}

bool AssetScanContext::isConsumed(const std::filesystem::path& file) const
{
    return m_consumed.contains(assetNormalizedPathKey(file));
}

void AssetScanContext::consume(const std::filesystem::path& file) const
{
    m_consumed.insert(assetNormalizedPathKey(file));
}

std::filesystem::path AssetScanContext::findSibling(const std::string& fileName) const
{
    const auto it = m_filesByName.find(assetLowerString(fileName));
    if (it == m_filesByName.end())
        return {};
    return it->second;
}

std::filesystem::path AssetScanContext::relativePath(const std::filesystem::path& path) const
{
    return assetRelativePath(m_root, path);
}

std::unique_ptr<Asset> AssetScanContext::createFileAsset(const std::filesystem::path& path) const
{
    std::error_code error;
    std::unique_ptr<Asset> result = AssetFileTypeRegistry::instance().createAssetForPath(path);
    result->absolutePath          = path.lexically_normal();
    result->relativePath          = relativePath(result->absolutePath);
    result->primaryFile           = result->relativePath;
    result->name                  = result->absolutePath.filename().generic_string();
    result->displayName           = result->name;
    result->sizeBytes             = std::filesystem::file_size(result->absolutePath, error);
    if (error)
        result->sizeBytes = 0;
    return result;
}
}  // namespace editor
