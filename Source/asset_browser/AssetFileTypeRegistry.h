#pragma once

#include "asset_browser/AssetKind.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace editor
{
class Asset;
class IEditorDocument;

struct AssetFileCreationDescriptor
{
    std::string menuLabel;
    std::string modalId;
    std::string modalTitle;
    std::string defaultFileName;
    bool openAfterCreate = false;
    std::function<bool(const std::filesystem::path&, std::string&)> createDefaultFile;
};

struct AssetFileTypeDescriptor
{
    AssetKind kind = AssetKind::Unknown;
    std::vector<std::string> extensions;
    std::function<std::unique_ptr<Asset>()> assetFactory;
    std::function<std::unique_ptr<IEditorDocument>()> documentFactory;
    std::optional<AssetFileCreationDescriptor> creation;
};

class AssetFileTypeRegistry
{
public:
    static const AssetFileTypeRegistry& instance();

    const AssetFileTypeDescriptor* findByKind(AssetKind kind) const;
    const AssetFileTypeDescriptor* findByExtension(const std::filesystem::path& path) const;
    std::unique_ptr<Asset> createAssetForPath(const std::filesystem::path& path) const;
    std::unique_ptr<IEditorDocument> createDocument(AssetKind kind) const;
    bool createDefaultFile(AssetKind kind, const std::filesystem::path& path, std::string& message) const;
    std::vector<const AssetFileTypeDescriptor*> creatableTypes() const;
    bool resolveCreateFileName(const AssetFileTypeDescriptor& type,
                               const std::string& requestedName,
                               std::filesystem::path& fileName,
                               std::string& message) const;

private:
    AssetFileTypeRegistry();

    std::vector<AssetFileTypeDescriptor> m_types;
};
}  // namespace editor
