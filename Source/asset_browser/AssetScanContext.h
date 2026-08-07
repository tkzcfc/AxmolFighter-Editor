#pragma once

#include "asset_browser/Asset.h"

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor
{
std::string assetLowerString(std::string value);
std::string assetLowerExtension(const std::filesystem::path& path);
std::string assetLowerFilename(const std::filesystem::path& path);
std::string assetNormalizedPathKey(const std::filesystem::path& path);
std::filesystem::path assetRelativePath(const std::filesystem::path& root, const std::filesystem::path& path);
bool sortAssets(const std::unique_ptr<Asset>& lhs, const std::unique_ptr<Asset>& rhs);

class AssetScanContext
{
public:
    AssetScanContext(const std::filesystem::path& root,
                     const std::vector<std::filesystem::path>& files,
                     const std::unordered_map<std::string, std::filesystem::path>& filesByName,
                     std::set<std::string>& consumed);

    const std::filesystem::path& root() const;
    const std::vector<std::filesystem::path>& files() const;

    bool isConsumed(const std::filesystem::path& file) const;
    void consume(const std::filesystem::path& file) const;
    std::filesystem::path findSibling(const std::string& fileName) const;
    std::filesystem::path relativePath(const std::filesystem::path& path) const;
    std::unique_ptr<Asset> createFileAsset(const std::filesystem::path& path) const;

private:
    const std::filesystem::path& m_root;
    const std::vector<std::filesystem::path>& m_files;
    const std::unordered_map<std::string, std::filesystem::path>& m_filesByName;
    std::set<std::string>& m_consumed;
};
}  // namespace editor
