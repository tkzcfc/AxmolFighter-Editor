#include "asset_browser/TextureAsset.h"

#include "ImGui/ImGuiPresenter.h"
#include "base/Director.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

namespace editor
{
namespace
{
constexpr const char* kIconImage = "\xef\x87\x85";  // fa-file-image

int readPngDimension(const unsigned char* bytes)
{
    return (static_cast<int>(bytes[0]) << 24) | (static_cast<int>(bytes[1]) << 16) |
           (static_cast<int>(bytes[2]) << 8) | static_cast<int>(bytes[3]);
}

bool readPngInfo(const std::filesystem::path& path, int& width, int& height)
{
    unsigned char header[24] = {};
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(header), sizeof(header)))
        return false;

    constexpr unsigned char kPngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (!std::equal(std::begin(kPngSignature), std::end(kPngSignature), header))
        return false;

    width = readPngDimension(header + 16);
    height = readPngDimension(header + 20);
    return width > 0 && height > 0;
}
}  // namespace

TextureAsset::TextureAsset() : Asset(AssetKind::Texture) {}

const char* TextureAsset::icon(bool) const
{
    return kIconImage;
}

void TextureAsset::drawTooltip() const
{
    int imageWidth = 0;
    int imageHeight = 0;
    const bool hasInfo = readPngInfo(absolutePath, imageWidth, imageHeight);

    ImGui::BeginTooltip();
    ImGui::TextUnformatted(displayName.c_str());
    ImGui::Separator();
    drawAssetBaseProperties(*this);
    if (hasInfo)
        ImGui::Text("Image: %d x %d px", imageWidth, imageHeight);
    else
        ImGui::TextDisabled("Image: unable to read PNG header");
    ImGui::EndTooltip();
}

void TextureAsset::drawPreview(EditorContext&, ImVec2 availableSize) const
{
    ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(relativePath.generic_string());
    if (!texture)
    {
        ImGui::TextDisabled("Unable to load texture.");
        ImGui::TextWrapped("%s", relativePath.generic_string().c_str());
        return;
    }

    const ImVec2 size = fitAssetPreviewSize(
        static_cast<float>(texture->getPixelsWide()), static_cast<float>(texture->getPixelsHigh()), availableSize);
    ax::extension::ImGuiPresenter::getInstance()->image(texture, size);
}

void TextureAsset::fillDragPayload(AssetDragPayload& payload) const
{
    Asset::fillDragPayload(payload);
    int imageWidth = 0;
    int imageHeight = 0;
    if (readPngInfo(absolutePath, imageWidth, imageHeight))
    {
        payload.width = imageWidth;
        payload.height = imageHeight;
        payload.sourceWidth = imageWidth;
        payload.sourceHeight = imageHeight;
    }
}
}  // namespace editor
