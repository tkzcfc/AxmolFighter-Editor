#include "asset_browser/AssetSelectionService.h"

#include <algorithm>

namespace editor
{
void AssetSelectionService::selectAsset(std::filesystem::path relativePath)
{
    m_selectedAssetPath = relativePath.lexically_normal();
    if (m_selectedAssetPath == ".")
        m_selectedAssetPath.clear();
    m_selectionAnchor = m_selectedAssetPath;
    m_selectedAssetPaths.clear();
    if (!m_selectedAssetPath.empty())
        m_selectedAssetPaths.push_back(m_selectedAssetPath);
}

void AssetSelectionService::toggleAsset(std::filesystem::path relativePath)
{
    relativePath  = relativePath.lexically_normal();
    const auto it = std::find(m_selectedAssetPaths.begin(), m_selectedAssetPaths.end(), relativePath);
    if (it == m_selectedAssetPaths.end())
    {
        m_selectedAssetPaths.push_back(relativePath);
        m_selectedAssetPath = relativePath;
    }
    else
    {
        m_selectedAssetPaths.erase(it);
        m_selectedAssetPath = m_selectedAssetPaths.empty() ? std::filesystem::path{} : m_selectedAssetPaths.back();
    }
    m_selectionAnchor = relativePath;
}

void AssetSelectionService::selectRange(std::filesystem::path relativePath,
                                        const std::vector<std::filesystem::path>& visiblePaths)
{
    relativePath = relativePath.lexically_normal();
    if (m_selectionAnchor.empty())
    {
        selectAsset(std::move(relativePath));
        return;
    }
    const auto anchorIt = std::find(visiblePaths.begin(), visiblePaths.end(), m_selectionAnchor);
    const auto targetIt = std::find(visiblePaths.begin(), visiblePaths.end(), relativePath);
    if (anchorIt == visiblePaths.end() || targetIt == visiblePaths.end())
    {
        selectAsset(std::move(relativePath));
        return;
    }
    const auto first = std::min(anchorIt, targetIt);
    const auto last  = std::max(anchorIt, targetIt);
    m_selectedAssetPaths.assign(first, last + 1);
    m_selectedAssetPath = relativePath;
}

void AssetSelectionService::clearSelection()
{
    m_selectedAssetPath.clear();
    m_selectionAnchor.clear();
    m_selectedAssetPaths.clear();
}

const std::vector<std::filesystem::path>& AssetSelectionService::selectedAssetPaths() const
{
    return m_selectedAssetPaths;
}

bool AssetSelectionService::isSelected(const std::filesystem::path& relativePath) const
{
    const std::filesystem::path normalized = relativePath.lexically_normal();
    return std::find(m_selectedAssetPaths.begin(), m_selectedAssetPaths.end(), normalized) !=
           m_selectedAssetPaths.end();
}

const std::filesystem::path& AssetSelectionService::selectedAssetPath() const
{
    return m_selectedAssetPath;
}

bool AssetSelectionService::hasSelection() const
{
    return !m_selectedAssetPath.empty();
}
}  // namespace editor
