#include "asset_browser/Asset.h"

#include "asset_browser/AssetScanContext.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace editor
{
namespace
{
constexpr const char* kIconFile = "\xef\x85\x9b";  // fa-file
}

Asset::Asset(AssetKind assetKind) : kind(assetKind) {}

bool Asset::isDirectory() const
{
    return false;
}

const char* Asset::icon(bool) const
{
    return kIconFile;
}

void Asset::drawTooltip() const
{
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    drawAssetBaseProperties(*this);
    ImGui::EndTooltip();
}

void Asset::drawPreview(EditorContext&, ImVec2) const
{
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    drawAssetBaseProperties(*this);
    ImGui::Spacing();
    ImGui::TextDisabled("No preview available.");
}

void Asset::fillDragPayload(AssetDragPayload& payload) const
{
    payload.kind = kind;
    const std::string relative = relativePath.generic_string();
    const std::string extension = assetLowerExtension(primaryFile);
    std::snprintf(payload.relativePath, sizeof(payload.relativePath), "%s", relative.c_str());
    std::snprintf(payload.extension, sizeof(payload.extension), "%s", extension.c_str());
}

bool Asset::canOpenDocument() const
{
    return false;
}

std::string formatAssetFileSize(std::uintmax_t bytes)
{
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);
    if (bytes >= static_cast<std::uintmax_t>(kMiB))
        stream << static_cast<double>(bytes) / kMiB << " MB";
    else if (bytes >= static_cast<std::uintmax_t>(kKiB))
        stream << static_cast<double>(bytes) / kKiB << " KB";
    else
        stream << bytes << " B";
    return stream.str();
}

void drawAssetBaseProperties(const Asset& asset)
{
    if (!asset.relativePath.empty())
        ImGui::Text("Path: %s", asset.relativePath.generic_string().c_str());
    if (!asset.primaryFile.empty() && asset.primaryFile != asset.relativePath)
        ImGui::Text("File: %s", asset.primaryFile.generic_string().c_str());
    if (asset.sizeBytes > 0)
        ImGui::Text("Size: %s", formatAssetFileSize(asset.sizeBytes).c_str());
}

ImVec2 fitAssetPreviewSize(float sourceWidth, float sourceHeight, ImVec2 availableSize)
{
    if (sourceWidth <= 0.0f || sourceHeight <= 0.0f)
        return ImVec2(std::max(1.0f, availableSize.x), std::max(1.0f, availableSize.y));

    const float maxWidth = std::max(1.0f, availableSize.x);
    const float maxHeight = std::max(1.0f, availableSize.y);
    const float scale = std::min(maxWidth / sourceWidth, maxHeight / sourceHeight);
    return ImVec2(std::max(1.0f, sourceWidth * scale), std::max(1.0f, sourceHeight * scale));
}
}  // namespace editor
