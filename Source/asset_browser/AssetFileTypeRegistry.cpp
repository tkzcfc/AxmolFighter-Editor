#include "asset_browser/AssetFileTypeRegistry.h"

#include "asset_browser/AniAsset.h"
#include "asset_browser/BoxAsset.h"
#include "asset_browser/BitmapFontAsset.h"
#include "asset_browser/LayerAsset.h"
#include "asset_browser/MotionAsset.h"
#include "asset_browser/TextAsset.h"
#include "asset_browser/TextureAsset.h"
#include "asset_browser/TrueTypeFontAsset.h"
#include "asset_browser/UnknownAsset.h"
#include "documents/AniDocument.h"
#include "documents/BoxDocument.h"
#include "documents/LayerDocument.h"
#include "documents/MotionDocument.h"
#include "documents/TextDocument.h"

#include <algorithm>
#include <cctype>

namespace editor
{
namespace
{
std::string lowerString(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool containsPathSeparator(const std::string& value)
{
    return value.find('/') != std::string::npos || value.find('\\') != std::string::npos;
}
}  // namespace

const AssetFileTypeRegistry& AssetFileTypeRegistry::instance()
{
    static AssetFileTypeRegistry registry;
    return registry;
}

AssetFileTypeRegistry::AssetFileTypeRegistry()
{
    m_types.push_back({AssetKind::Texture, {".png"}, [] { return std::make_unique<TextureAsset>(); }});
    m_types.push_back({AssetKind::Layer,
                       {".layer"},
                       [] { return std::make_unique<LayerAsset>(); },
                       [] { return std::make_unique<LayerDocument>(); },
                       AssetFileCreationDescriptor{"Layer File", "asset.create-layer", "Create Layer File",
                                                   "NewLayer.layer", false, LayerDocument::createDefaultFile}});
    m_types.push_back(
        {AssetKind::FrameAnimation,
         {".ani"},
         [] { return std::make_unique<AniAsset>(); },
         [] { return std::make_unique<AniDocument>(); },
         AssetFileCreationDescriptor{"Frame Animation", "asset.create-animation", "Create Frame Animation",
                                     "NewAnimation.ani", true, AniDocument::createDefaultFile}});
    m_types.push_back(
        {AssetKind::Box,
         {".box"},
         [] { return std::make_unique<BoxAsset>(); },
         [] { return std::make_unique<BoxDocument>(); },
         AssetFileCreationDescriptor{"Combat Box", "asset.create-box", "Create Combat Box", "NewBox.box", true,
                                     BoxDocument::createDefaultFile}});
    m_types.push_back(
        {AssetKind::Motion,
         {".motion"},
         [] { return std::make_unique<MotionAsset>(); },
         [] { return std::make_unique<MotionDocument>(); },
         AssetFileCreationDescriptor{"Motion Mapping", "asset.create-motion", "Create Motion Mapping",
                                     "NewMotion.motion", true, MotionDocument::createDefaultFile}});
    m_types.push_back({AssetKind::BitmapFont, {".fnt"}, [] { return std::make_unique<BitmapFontAsset>(); }});
    m_types.push_back(
        {AssetKind::TrueTypeFont, {".ttf", ".otf"}, [] { return std::make_unique<TrueTypeFontAsset>(); }});
    m_types.push_back({AssetKind::Text,
                       {".txt", ".json", ".md", ".ini", ".cfg", ".lua", ".glsl", ".vert", ".frag", ".plist", ".atlas"},
                       [] { return std::make_unique<TextAsset>(); },
                       [] { return std::make_unique<TextDocument>(); }});
}

const AssetFileTypeDescriptor* AssetFileTypeRegistry::findByKind(AssetKind kind) const
{
    const auto it = std::find_if(m_types.begin(), m_types.end(),
                                 [kind](const AssetFileTypeDescriptor& type) { return type.kind == kind; });
    return it == m_types.end() ? nullptr : &*it;
}

const AssetFileTypeDescriptor* AssetFileTypeRegistry::findByExtension(const std::filesystem::path& path) const
{
    const std::string extension = lowerString(path.extension().generic_string());
    for (const AssetFileTypeDescriptor& type : m_types)
    {
        if (std::find(type.extensions.begin(), type.extensions.end(), extension) != type.extensions.end())
            return &type;
    }
    return nullptr;
}

std::unique_ptr<Asset> AssetFileTypeRegistry::createAssetForPath(const std::filesystem::path& path) const
{
    const AssetFileTypeDescriptor* type = findByExtension(path);
    if (!type || !type->assetFactory)
        return std::make_unique<UnknownAsset>();
    return type->assetFactory();
}

std::unique_ptr<IEditorDocument> AssetFileTypeRegistry::createDocument(AssetKind kind) const
{
    const AssetFileTypeDescriptor* type = findByKind(kind);
    return type && type->documentFactory ? type->documentFactory() : nullptr;
}

bool AssetFileTypeRegistry::createDefaultFile(AssetKind kind,
                                              const std::filesystem::path& path,
                                              std::string& message) const
{
    const AssetFileTypeDescriptor* type = findByKind(kind);
    if (!type || !type->creation || !type->creation->createDefaultFile)
    {
        message = "Asset type cannot be created.";
        return false;
    }
    return type->creation->createDefaultFile(path, message);
}

std::vector<const AssetFileTypeDescriptor*> AssetFileTypeRegistry::creatableTypes() const
{
    std::vector<const AssetFileTypeDescriptor*> result;
    for (const AssetFileTypeDescriptor& type : m_types)
    {
        if (type.creation && type.creation->createDefaultFile)
            result.push_back(&type);
    }
    return result;
}

bool AssetFileTypeRegistry::resolveCreateFileName(const AssetFileTypeDescriptor& type,
                                                  const std::string& requestedName,
                                                  std::filesystem::path& fileName,
                                                  std::string& message) const
{
    message.clear();
    if (!type.creation || type.extensions.empty())
    {
        message = "Asset type cannot be created.";
        return false;
    }
    if (requestedName.empty())
    {
        message = "File name is required.";
        return false;
    }
    if (containsPathSeparator(requestedName))
    {
        message = "File name cannot contain path separators.";
        return false;
    }

    std::string normalizedName = requestedName;
    const std::filesystem::path requestedPath(requestedName);
    if (lowerString(requestedPath.extension().generic_string()) != type.extensions.front())
        normalizedName += type.extensions.front();
    fileName = std::filesystem::path(normalizedName).filename();
    if (fileName.empty() || fileName == "." || fileName == ".." || fileName.stem().empty())
    {
        message = "File name is invalid.";
        return false;
    }
    return true;
}
}  // namespace editor
