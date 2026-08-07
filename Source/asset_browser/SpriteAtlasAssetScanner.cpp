#include "asset_browser/SpriteAtlasAssetScanner.h"

#include "asset_browser/AssetScanContext.h"
#include "asset_browser/SpriteAtlasAsset.h"
#include "asset_browser/SpriteFrameAsset.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

namespace editor
{
namespace
{
struct ParsedSpriteFrame
{
    std::string name;
    int width = 0;
    int height = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
};

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool parseSizeString(const std::string& text, int& width, int& height)
{
    static const std::regex sizePattern(R"(\{\s*(-?\d+)\s*,\s*(-?\d+)\s*\})");
    std::smatch match;
    if (!std::regex_search(text, match, sizePattern) || match.size() < 3)
        return false;

    width = std::stoi(match[1].str());
    height = std::stoi(match[2].str());
    return width > 0 && height > 0;
}

bool parseFrameSize(const std::string& block, int& width, int& height)
{
    static const std::regex framePattern(
        R"(<key>frame</key>\s*<string>\s*\{\s*\{\s*-?\d+\s*,\s*-?\d+\s*\}\s*,\s*\{\s*(\d+)\s*,\s*(\d+)\s*\}\s*\}\s*</string>)");
    std::smatch match;
    if (!std::regex_search(block, match, framePattern) || match.size() < 3)
        return false;

    width = std::stoi(match[1].str());
    height = std::stoi(match[2].str());
    return width > 0 && height > 0;
}

bool parseSourceSize(const std::string& block, int& width, int& height)
{
    static const std::regex sourcePattern(R"(<key>sourceSize</key>\s*<string>([^<]+)</string>)");
    std::smatch match;
    if (!std::regex_search(block, match, sourcePattern) || match.size() < 2)
        return false;

    return parseSizeString(match[1].str(), width, height);
}

bool parseSpriteAtlas(const std::filesystem::path& plistPath,
                      std::string& textureFileName,
                      std::vector<ParsedSpriteFrame>& frames)
{
    const std::string content = readTextFile(plistPath);
    if (content.empty())
        return false;

    static const std::regex texturePattern(
        R"(<key>(?:realTextureFileName|textureFileName)</key>\s*<string>([^<]+)</string>)");
    std::smatch textureMatch;
    if (!std::regex_search(content, textureMatch, texturePattern) || textureMatch.size() < 2)
        return false;
    textureFileName = textureMatch[1].str();

    const std::size_t framesKey = content.find("<key>frames</key>");
    const std::size_t metadataKey = content.find("<key>metadata</key>");
    if (framesKey == std::string::npos || metadataKey == std::string::npos || metadataKey <= framesKey)
        return false;

    const std::size_t framesDictStart = content.find("<dict>", framesKey);
    if (framesDictStart == std::string::npos || framesDictStart >= metadataKey)
        return false;

    const std::size_t framesContentStart = framesDictStart + 6;
    if (framesContentStart >= metadataKey)
        return false;

    const std::string framesSection = content.substr(framesContentStart, metadataKey - framesContentStart);
    static const std::regex frameKeyPattern(R"(<key>([^<]+)</key>\s*<dict>)");
    for (std::sregex_iterator it(framesSection.begin(), framesSection.end(), frameKeyPattern), end; it != end; ++it)
    {
        const std::smatch& match = *it;
        const std::string frameName = match[1].str();
        const std::size_t blockStart = static_cast<std::size_t>(match.position() + match.length());
        const std::size_t blockEnd = framesSection.find("</dict>", blockStart);
        if (blockEnd == std::string::npos)
            continue;

        const std::string block = framesSection.substr(blockStart, blockEnd - blockStart);
        ParsedSpriteFrame frame;
        frame.name = frameName;
        parseFrameSize(block, frame.width, frame.height);
        parseSourceSize(block, frame.sourceWidth, frame.sourceHeight);
        if (frame.sourceWidth <= 0)
            frame.sourceWidth = frame.width;
        if (frame.sourceHeight <= 0)
            frame.sourceHeight = frame.height;
        frames.push_back(std::move(frame));
    }

    return !textureFileName.empty() && !frames.empty();
}
}  // namespace

void SpriteAtlasAssetScanner::scan(const AssetScanContext& context, std::vector<std::unique_ptr<Asset>>& output) const
{
    std::error_code error;
    for (const std::filesystem::path& file : context.files())
    {
        if (assetLowerExtension(file) != ".plist" || context.isConsumed(file))
            continue;

        std::string textureFileName;
        std::vector<ParsedSpriteFrame> frames;
        if (!parseSpriteAtlas(file, textureFileName, frames))
            continue;

        const std::filesystem::path texturePath =
            context.findSibling(std::filesystem::path(textureFileName).filename().generic_string());
        if (texturePath.empty())
            continue;

        auto atlas = std::make_unique<SpriteAtlasAsset>();
        atlas->name = file.filename().generic_string();
        atlas->displayName = atlas->name;
        atlas->relativePath = context.relativePath(file);
        atlas->primaryFile = atlas->relativePath;
        atlas->absolutePath = file;
        atlas->atlasPath = atlas->relativePath;
        atlas->sizeBytes = std::filesystem::file_size(file, error);
        if (error)
            atlas->sizeBytes = 0;
        atlas->relatedFiles.push_back(context.relativePath(texturePath));

        for (const ParsedSpriteFrame& parsedFrame : frames)
        {
            auto frame = std::make_unique<SpriteFrameAsset>();
            frame->name = std::filesystem::path(parsedFrame.name).filename().generic_string();
            frame->displayName = parsedFrame.name;
            frame->relativePath = std::filesystem::path(atlas->relativePath.generic_string() + "#" + parsedFrame.name);
            frame->primaryFile = atlas->relativePath;
            frame->absolutePath = file;
            frame->atlasPath = atlas->relativePath;
            frame->frameName = parsedFrame.name;
            frame->width = parsedFrame.width;
            frame->height = parsedFrame.height;
            frame->sourceWidth = parsedFrame.sourceWidth;
            frame->sourceHeight = parsedFrame.sourceHeight;
            atlas->children.push_back(std::move(frame));
        }
        std::sort(atlas->children.begin(), atlas->children.end(), sortAssets);

        context.consume(file);
        context.consume(texturePath);
        output.push_back(std::move(atlas));
    }
}
}  // namespace editor
