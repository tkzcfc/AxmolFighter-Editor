#pragma once

#include "asset_browser/AssetKind.h"
#include "modules/PanelModule.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace editor
{
class Asset;
class AssetBrowserService;
struct AssetFileTypeDescriptor;

class AssetBrowserModule : public PanelModule
{
public:
    AssetBrowserModule();

private:
    void drawContent(EditorContext& context) override;
    void drawBrowser(EditorContext& context, AssetBrowserService& browser);
    void drawAssetNode(EditorContext& context,
                       AssetBrowserService& browser,
                       const std::filesystem::path& root,
                       const Asset& asset);
    void collectVisibleAssetPaths(const Asset& asset);
    void drawAssetContextMenu(EditorContext& context,
                              AssetBrowserService& browser,
                              const std::filesystem::path& root,
                              const char* popupId,
                              const std::filesystem::path& targetDirectory,
                              bool allowCreate,
                              const Asset* asset = nullptr);
    void requestCreateAsset(EditorContext& context,
                            const AssetFileTypeDescriptor& type,
                            const std::filesystem::path& targetDirectory);
    void requestDuplicateLayer(EditorContext& context, const Asset& asset);
    void requestRenameLayer(EditorContext& context, const Asset& asset);
    void requestDeleteLayer(EditorContext& context, const Asset& asset);
    bool createAssetFile(EditorContext& context);
    bool duplicateLayerFile(EditorContext& context);
    bool renameLayerFile(EditorContext& context);
    bool deleteLayerFile(EditorContext& context);
    void openAsset(EditorContext& context, const Asset& asset);
    void drawIcon(const char* icon) const;
    bool drawDisclosureTriangle(bool open) const;
    void drawModalMessage(EditorContext& context) const;
    void setModalMessage(EditorContext& context, std::string message) const;
    std::filesystem::path makeUniqueAssetPath(const std::filesystem::path& directory,
                                              const std::string& fileName) const;
    std::string nextDuplicateLayerFileName(const std::filesystem::path& sourcePath) const;
    bool resolveLayerFileName(const std::string& requestedName,
                              std::filesystem::path& fileName,
                              std::string& message) const;
    std::filesystem::path assetRoot(EditorContext& context) const;

    void saveState(EditorContext& context);

    std::filesystem::path m_createTargetDirectory;
    std::filesystem::path m_duplicateSourceRelativePath;
    std::filesystem::path m_renameSourceRelativePath;
    std::filesystem::path m_deleteSourceRelativePath;
    AssetKind m_createAssetKind = AssetKind::Unknown;
    std::string m_createAssetName;
    std::string m_duplicateLayerName;
    std::string m_renameLayerName;
    std::string m_statusMessage;
    std::unordered_set<std::string> m_openAssets;
    std::vector<std::filesystem::path> m_visibleAssetPaths;
    bool m_rootOpen       = true;
    bool m_pendingRefresh = false;
    bool m_stateLoaded    = false;
    std::string m_loadedRoot;
};
}  // namespace editor
