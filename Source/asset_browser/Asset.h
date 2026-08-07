#pragma once

#include "asset_browser/AssetDragPayload.h"

#include "imgui.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace editor
{
class EditorContext;

class Asset
{
public:
    explicit Asset(AssetKind kind);
    virtual ~Asset() = default;

    virtual bool isDirectory() const;
    virtual const char* icon(bool open) const;
    virtual void drawTooltip() const;
    virtual void drawPreview(EditorContext& context, ImVec2 availableSize) const;
    virtual void fillDragPayload(AssetDragPayload& payload) const;
    virtual bool canOpenDocument() const;

    std::string name;
    std::string displayName;
    AssetKind kind = AssetKind::Unknown;
    std::filesystem::path relativePath;
    std::filesystem::path primaryFile;
    std::filesystem::path absolutePath;
    std::uintmax_t sizeBytes = 0;
    std::vector<std::filesystem::path> relatedFiles;
    std::vector<std::unique_ptr<Asset>> children;
};

std::string formatAssetFileSize(std::uintmax_t bytes);
void drawAssetBaseProperties(const Asset& asset);
ImVec2 fitAssetPreviewSize(float sourceWidth, float sourceHeight, ImVec2 availableSize);
}  // namespace editor
