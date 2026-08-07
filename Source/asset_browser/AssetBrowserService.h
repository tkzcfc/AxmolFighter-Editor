#pragma once

#include "asset_browser/FolderAsset.h"
#include "core/IService.h"

#include <filesystem>
#include <memory>
#include <string>

namespace editor
{
class AssetBrowserService final : public IService
{
public:
    bool rootExists(const std::filesystem::path& root) const;
    bool refresh(const std::filesystem::path& root, std::string* message = nullptr);
    const Asset& rootAsset(const std::filesystem::path& root);
    const Asset* findAsset(const std::filesystem::path& relativePath) const;
    bool isScanning() const;
    const std::string& statusMessage() const;
    std::filesystem::path normalizeRelativeDirectory(const std::filesystem::path& relativeDirectory) const;

private:
    std::unique_ptr<FolderAsset> buildDirectory(const std::filesystem::path& root,
                                                const std::filesystem::path& directory) const;
    const Asset* findAssetRecursive(const Asset& asset, const std::filesystem::path& relativePath) const;
    void resetPlaceholderRoot(const std::filesystem::path& root);

    std::filesystem::path m_root;
    std::unique_ptr<FolderAsset> m_rootAsset;
    std::string m_statusMessage;
    std::uint64_t m_scanGeneration = 0;
    bool m_scanned = false;
    bool m_scanning = false;
};
}  // namespace editor
