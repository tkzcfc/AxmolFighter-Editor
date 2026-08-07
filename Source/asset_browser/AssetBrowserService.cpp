#include "asset_browser/AssetBrowserService.h"

#include "asset_browser/AssetScanContext.h"
#include "asset_browser/FolderAsset.h"
#include "asset_browser/IAssetScanner.h"
#include "base/Director.h"
#include "base/JobSystem.h"

#include <algorithm>
#include <memory>
#include <set>
#include <system_error>
#include <unordered_map>

namespace editor
{
namespace
{
struct AssetScanResult
{
    std::unique_ptr<FolderAsset> rootAsset;
    std::string message;
    bool success = false;
};
}  // namespace

bool AssetBrowserService::rootExists(const std::filesystem::path& root) const
{
    std::error_code error;
    return std::filesystem::exists(root, error) && std::filesystem::is_directory(root, error);
}

bool AssetBrowserService::refresh(const std::filesystem::path& root, std::string* message)
{
    if (message)
        message->clear();

    if (!rootExists(root))
    {
        if (message)
            *message = "Content root does not exist: " + root.generic_string();
        m_statusMessage = message ? *message : "Content root does not exist.";
        resetPlaceholderRoot(root);
        m_scanned = false;
        m_scanning = false;
        return false;
    }

    const std::filesystem::path normalizedRoot = root.lexically_normal();
    m_root = normalizedRoot;
    m_scanning = true;
    m_statusMessage = "Scanning assets...";
    if (!m_rootAsset)
        resetPlaceholderRoot(normalizedRoot);

    const std::uint64_t generation = ++m_scanGeneration;
    auto result = std::make_shared<AssetScanResult>();
    ax::Director::getInstance()->getJobSystem()->enqueue(
        [this, normalizedRoot, result] {
            try
            {
                result->rootAsset = buildDirectory(normalizedRoot, normalizedRoot);
                result->rootAsset->name = "Content";
                result->rootAsset->displayName = "Content";
                result->rootAsset->relativePath.clear();
                result->rootAsset->primaryFile.clear();
                result->rootAsset->absolutePath = normalizedRoot;
                result->success = true;
                result->message = "Assets refreshed.";
            }
            catch (const std::exception& exception)
            {
                result->success = false;
                result->message = std::string("Asset scan failed: ") + exception.what();
            }
            catch (...)
            {
                result->success = false;
                result->message = "Asset scan failed: unknown error.";
            }
        },
        [this, normalizedRoot, generation, result] {
            if (generation != m_scanGeneration || normalizedRoot != m_root)
                return;

            m_scanning = false;
            m_statusMessage = result->message;
            if (result->success)
            {
                m_rootAsset = std::move(result->rootAsset);
                m_scanned = true;
            }
        });

    if (message)
        *message = m_statusMessage;
    return true;
}

const Asset& AssetBrowserService::rootAsset(const std::filesystem::path& root)
{
    const std::filesystem::path normalizedRoot = root.lexically_normal();
    if (m_root != normalizedRoot || (!m_scanned && !m_scanning))
    {
        std::string ignored;
        refresh(normalizedRoot, &ignored);
    }

    if (!m_rootAsset)
        resetPlaceholderRoot(normalizedRoot);
    return *m_rootAsset;
}

const Asset* AssetBrowserService::findAsset(const std::filesystem::path& relativePath) const
{
    if (!m_scanned || !m_rootAsset)
        return nullptr;
    return findAssetRecursive(*m_rootAsset, relativePath.lexically_normal());
}

std::filesystem::path AssetBrowserService::normalizeRelativeDirectory(
    const std::filesystem::path& relativeDirectory) const
{
    const std::filesystem::path normalized = relativeDirectory.lexically_normal();
    if (normalized.empty() || normalized == ".")
        return {};
    if (normalized.is_absolute())
        return {};

    for (const auto& part : normalized)
    {
        if (part == "..")
            return {};
    }
    return normalized;
}

bool AssetBrowserService::isScanning() const
{
    return m_scanning;
}

const std::string& AssetBrowserService::statusMessage() const
{
    return m_statusMessage;
}

std::unique_ptr<FolderAsset> AssetBrowserService::buildDirectory(const std::filesystem::path& root,
                                                                 const std::filesystem::path& directory) const
{
    auto directoryAsset = std::make_unique<FolderAsset>();
    directoryAsset->absolutePath = directory.lexically_normal();
    directoryAsset->relativePath = assetRelativePath(root, directory);
    directoryAsset->primaryFile = directoryAsset->relativePath;
    directoryAsset->name = directory.filename().generic_string();
    directoryAsset->displayName = directoryAsset->name;

    std::error_code error;
    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error)
            break;

        if (entry.is_directory(error))
        {
            directoryAsset->children.push_back(buildDirectory(root, entry.path()));
        }
        else if (!error)
        {
            files.push_back(entry.path().lexically_normal());
        }
    }

    std::unordered_map<std::string, std::filesystem::path> filesByName;
    for (const std::filesystem::path& file : files)
        filesByName[assetLowerFilename(file)] = file;

    std::set<std::string> consumed;
    AssetScanContext context(root, files, filesByName, consumed);
    const std::vector<std::unique_ptr<IAssetScanner>> scanners = createDefaultAssetScanners();
    for (const std::unique_ptr<IAssetScanner>& scanner : scanners)
        scanner->scan(context, directoryAsset->children);

    std::sort(directoryAsset->children.begin(), directoryAsset->children.end(), sortAssets);
    return directoryAsset;
}

const Asset* AssetBrowserService::findAssetRecursive(const Asset& asset,
                                                     const std::filesystem::path& relativePath) const
{
    if (asset.relativePath == relativePath)
        return &asset;

    const std::string target = relativePath.generic_string();
    if (asset.relativePath.generic_string() == target)
        return &asset;

    for (const std::unique_ptr<Asset>& child : asset.children)
    {
        if (child)
        {
            if (const Asset* result = findAssetRecursive(*child, relativePath))
                return result;
        }
    }
    return nullptr;
}

void AssetBrowserService::resetPlaceholderRoot(const std::filesystem::path& root)
{
    m_root = root.lexically_normal();
    m_rootAsset = std::make_unique<FolderAsset>();
    m_rootAsset->name = "Content";
    m_rootAsset->displayName = "Content";
    m_rootAsset->absolutePath = m_root;
}
}  // namespace editor
