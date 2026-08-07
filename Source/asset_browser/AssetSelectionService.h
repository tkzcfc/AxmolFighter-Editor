#pragma once

#include "core/IService.h"

#include <filesystem>
#include <vector>

namespace editor
{
class AssetSelectionService final : public IService
{
public:
    void selectAsset(std::filesystem::path relativePath);
    void toggleAsset(std::filesystem::path relativePath);
    void selectRange(std::filesystem::path relativePath, const std::vector<std::filesystem::path>& visiblePaths);
    void clearSelection();
    const std::filesystem::path& selectedAssetPath() const;
    const std::vector<std::filesystem::path>& selectedAssetPaths() const;
    bool isSelected(const std::filesystem::path& relativePath) const;
    bool hasSelection() const;

private:
    std::filesystem::path m_selectedAssetPath;
    std::filesystem::path m_selectionAnchor;
    std::vector<std::filesystem::path> m_selectedAssetPaths;
};
}  // namespace editor
