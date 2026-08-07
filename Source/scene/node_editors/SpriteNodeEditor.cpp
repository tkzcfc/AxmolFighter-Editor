#include "scene/node_editors/SpriteNodeEditor.h"

#include "2d/Sprite.h"
#include "2d/SpriteFrameCache.h"
#include "2d/SpriteFrame.h"
#include "base/Director.h"
#include "core/SignatureUtils.h"
#include "imgui.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"
#include "scene/node_editors/SpriteRenderUtils.h"
#include "editor_properties/AssetReferenceEditorProperty.h"
#include "editor_properties/ButtonEditorProperty.h"
#include "editor_properties/EnumEditorProperty.h"
#include "editor_properties/SpriteSliceEditorProperty.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"

#include <algorithm>

namespace editor
{
namespace
{
SceneVec2 jsonVec2Or(const rapidjson::Value& object, const char* key, SceneVec2 fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
        return fallback;

    const rapidjson::Value& value = object[key];
    fallback.x = jsonNumberOr(value, "x", fallback.x);
    fallback.y = jsonNumberOr(value, "y", fallback.y);
    return fallback;
}

void writeVec2(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const char* key, const SceneVec2& value)
{
    writer.Key(key);
    writer.StartObject();
    writer.Key("x");
    writer.Double(value.x);
    writer.Key("y");
    writer.Double(value.y);
    writer.EndObject();
}

void applySpriteSourceSizeFromPayload(SceneNode& node, const AssetDragPayload& asset)
{
    const int width = asset.sourceWidth > 0 ? asset.sourceWidth : asset.width;
    const int height = asset.sourceHeight > 0 ? asset.sourceHeight : asset.height;
    if (width > 0 && height > 0)
        node.size = {static_cast<float>(width), static_cast<float>(height)};
}

bool applySpriteCurrentSourceSize(SceneNode& node)
{
    if (node.sprite.sourceType == "SpriteFrame")
    {
        if (node.sprite.atlasPath.empty() || node.sprite.frameName.empty())
            return false;

        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.sprite.atlasPath))
            cache->addSpriteFramesWithFile(node.sprite.atlasPath);

        ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.sprite.frameName);
        if (!frame)
            return false;

        const ax::Vec2 size = frame->getOriginalSize();
        if (size.x <= 0.0f || size.y <= 0.0f)
            return false;
        node.size = {size.x, size.y};
        return true;
    }

    if (node.sprite.imagePath.empty())
        return false;

    ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(node.sprite.imagePath);
    if (!texture)
        return false;

    node.size = {static_cast<float>(texture->getPixelsWide()), static_cast<float>(texture->getPixelsHigh())};
    return node.size.width > 0.0f && node.size.height > 0.0f;
}

void assignSpriteAsset(SceneNode& node, const AssetDragPayload& asset)
{
    if (asset.kind == AssetKind::SpriteFrame)
    {
        node.sprite.sourceType = "SpriteFrame";
        node.sprite.atlasPath = asset.atlasPath;
        node.sprite.frameName = asset.frameName;
        node.sprite.imagePath.clear();
        applySpriteSourceSizeFromPayload(node, asset);
        return;
    }

    if (asset.kind == AssetKind::Texture)
    {
        node.sprite.sourceType = "Texture";
        node.sprite.imagePath = asset.relativePath;
        node.sprite.atlasPath.clear();
        node.sprite.frameName.clear();
        applySpriteSourceSizeFromPayload(node, asset);
    }
}

void prepareSpriteContentSize(ax::Sprite& sprite, const SceneNode& node)
{
    sprite.setContentSize(ax::Vec2(std::max(1.0f, node.size.width), std::max(1.0f, node.size.height)));
}

ax::Texture2D* currentSpriteTexture(const SceneNode& node)
{
    if (node.sprite.sourceType == "SpriteFrame")
    {
        if (node.sprite.atlasPath.empty() || node.sprite.frameName.empty())
            return nullptr;

        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.sprite.atlasPath))
            cache->addSpriteFramesWithFile(node.sprite.atlasPath);

        ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.sprite.frameName);
        return frame ? frame->getTexture() : nullptr;
    }

    if (node.sprite.imagePath.empty())
        return nullptr;

    return ax::Director::getInstance()->getTextureCache()->addImage(node.sprite.imagePath);
}

std::string effectiveDefaultBlendSrc(const SceneNode& node)
{
    ax::Texture2D* texture = currentSpriteTexture(node);
    return texture && texture->hasPremultipliedAlpha() ? "ONE" : "SRC_ALPHA";
}

std::string effectiveDefaultBlendDst()
{
    return "ONE_MINUS_SRC_ALPHA";
}
}  // namespace

std::string_view SpriteNodeEditor::typeId() const
{
    return "Sprite";
}

std::string_view SpriteNodeEditor::displayName() const
{
    return "Sprite";
}

std::string_view SpriteNodeEditor::defaultName() const
{
    return "Sprite";
}

void SpriteNodeEditor::readCustomData(SceneNode& node, const rapidjson::Value& value) const
{
    if (value.HasMember("sprite") && value["sprite"].IsObject())
    {
        const rapidjson::Value& sprite = value["sprite"];
        node.sprite.sourceType = jsonStringOr(sprite, "sourceType", node.sprite.sourceType);
        node.sprite.imagePath = jsonStringOr(sprite, "imagePath", node.sprite.imagePath);
        node.sprite.atlasPath = jsonStringOr(sprite, "atlasPath", node.sprite.atlasPath);
        node.sprite.frameName = jsonStringOr(sprite, "frameName", node.sprite.frameName);
        node.sprite.renderType = normalizeSpriteRenderType(jsonStringOr(sprite, "renderType", node.sprite.renderType));
        node.sprite.blendSrc = normalizeSpriteBlendFactor(jsonStringOr(sprite, "blendSrc", node.sprite.blendSrc), "");
        node.sprite.blendDst =
            normalizeSpriteBlendFactor(jsonStringOr(sprite, "blendDst", node.sprite.blendDst), "");
        node.sprite.sliceCenterOrigin = jsonVec2Or(sprite, "sliceCenterOrigin", node.sprite.sliceCenterOrigin);
        node.sprite.sliceCenterSize = jsonVec2Or(sprite, "sliceCenterSize", node.sprite.sliceCenterSize);
    }
}

void SpriteNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                            const SceneNode& node) const
{
    writer.Key("sprite");
    writer.StartObject();
    writer.Key("sourceType");
    writer.String(node.sprite.sourceType.c_str());
    writer.Key("imagePath");
    writer.String(node.sprite.imagePath.c_str());
    writer.Key("atlasPath");
    writer.String(node.sprite.atlasPath.c_str());
    writer.Key("frameName");
    writer.String(node.sprite.frameName.c_str());
    writer.Key("renderType");
    writer.String(normalizeSpriteRenderType(node.sprite.renderType).c_str());
    writer.Key("blendSrc");
    writer.String(normalizeSpriteBlendFactor(node.sprite.blendSrc, "").c_str());
    writer.Key("blendDst");
    writer.String(normalizeSpriteBlendFactor(node.sprite.blendDst, "").c_str());
    writeVec2(writer, "sliceCenterOrigin", node.sprite.sliceCenterOrigin);
    writeVec2(writer, "sliceCenterSize", node.sprite.sliceCenterSize);
    writer.EndObject();
}

void SpriteNodeEditor::appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const
{
    BaseNodeEditor::appendPropertyGroups(node, groups);

    EditorPropertyGroup group;
    group.label = "Sprite";
    const std::string editPrefix = node.id + ":sprite.";
    group.properties.push_back(std::make_unique<EnumEditorProperty>(
        editPrefix + "sourceType", "Source Type", node.sprite.sourceType, std::vector<std::string>{"Texture", "SpriteFrame"}));
    group.properties.push_back(
        std::make_unique<EnumEditorProperty>(editPrefix + "renderType", "Render Type", node.sprite.renderType, spriteRenderTypeNames()));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(editPrefix + "blendSrc",
                                                                    "Blend Src",
                                                                    node.sprite.blendSrc,
                                                                    spriteBlendFactorNames(),
                                                                    effectiveDefaultBlendSrc(node)));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(editPrefix + "blendDst",
                                                                    "Blend Dst",
                                                                    node.sprite.blendDst,
                                                                    spriteBlendFactorNames(),
                                                                    effectiveDefaultBlendDst()));
    if (normalizeSpriteRenderType(node.sprite.renderType) == "Sliced")
    {
        group.properties.push_back(
            std::make_unique<SpriteSliceEditorProperty>(editPrefix + "slice", "Sliced Borders"));
    }

    if (node.sprite.sourceType == "SpriteFrame")
    {
        auto assignSpriteFrame = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
            assignSpriteAsset(context.node, asset);
        };
        group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "atlasPath",
                                                                                 "Atlas Path",
                                                                                 node.sprite.atlasPath,
                                                                                 std::initializer_list<AssetKind>{AssetKind::Texture, AssetKind::SpriteFrame},
                                                                                 assignSpriteFrame));
        group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "frameName",
                                                                                 "Frame Name",
                                                                                 node.sprite.frameName,
                                                                                 std::initializer_list<AssetKind>{AssetKind::Texture, AssetKind::SpriteFrame},
                                                                                 assignSpriteFrame));
    }
    else
    {
        auto assignTexture = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
            assignSpriteAsset(context.node, asset);
        };
        group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "imagePath",
                                                                                 "Image Path",
                                                                                 node.sprite.imagePath,
                                                                                 std::initializer_list<AssetKind>{AssetKind::Texture, AssetKind::SpriteFrame},
                                                                                 assignTexture));
    }

    group.properties.push_back(std::make_unique<ButtonEditorProperty>(editPrefix + "setSizeToSource",
                                                                     "Set Size To Source",
                                                                     [](EditorPropertyContext& context) {
                                                                         context.document.beginUndoTransaction();
                                                                         if (applySpriteCurrentSourceSize(context.node))
                                                                             context.document.markDirty();
                                                                         context.document.commitUndoTransaction();
                                                                     }));
    groups.push_back(std::move(group));
}

std::tuple<std::string, ax::Node*> SpriteNodeEditor::createEngineNode(const SceneNode& node,
                                                                           const SceneNodeRuntimeContext&) const
{
    if (node.sprite.sourceType == "SpriteFrame")
    {
        if (node.sprite.atlasPath.empty())
            return {"Sprite atlas path is empty", nullptr};
        if (node.sprite.frameName.empty())
            return {"Sprite frame name is empty", nullptr};

        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.sprite.atlasPath))
            cache->addSpriteFramesWithFile(node.sprite.atlasPath);
        if (!cache->getSpriteFrameByName(node.sprite.frameName))
            return {"Failed to load sprite frame: " + node.sprite.frameName, nullptr};

        ax::Sprite* sprite = ax::Sprite::createWithSpriteFrameName(node.sprite.frameName);
        if (!sprite || !sprite->getTexture())
            return {"Failed to create sprite frame: " + node.sprite.frameName, nullptr};

        prepareSpriteContentSize(*sprite, node);
        applySpriteRenderSettings(*sprite, node.sprite);
        return {"", sprite};
    }

    if (node.sprite.imagePath.empty())
        return {"Image path is empty", nullptr};
    ax::Sprite* sprite = ax::Sprite::create(node.sprite.imagePath);
    if (!sprite || !sprite->getTexture())
        return {"Failed to load image: " + node.sprite.imagePath, nullptr};

    prepareSpriteContentSize(*sprite, node);
    applySpriteRenderSettings(*sprite, node.sprite);
    return {"", sprite};
}

std::string SpriteNodeEditor::engineSignature(const SceneNode& node, const SceneNodeRuntimeContext&) const
{
    return SignatureBuilder()
        .appendRaw("Sprite")
        .appendString("renderType", normalizeSpriteRenderType(node.sprite.renderType))
        .hash();
}

bool SpriteNodeEditor::updateEngineNode(ax::Node& runtimeNode,
                                             const SceneNode& node,
                                             const SceneNodeRuntimeContext&) const
{
    auto* sprite = dynamic_cast<ax::Sprite*>(&runtimeNode);
    if (!sprite)
        return false;

    if (normalizeSpriteRenderType(node.sprite.renderType) == "Tiled")
        return false;

    if (node.sprite.sourceType == "SpriteFrame")
    {
        if (node.sprite.atlasPath.empty() || node.sprite.frameName.empty())
            return false;

        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.sprite.atlasPath))
            cache->addSpriteFramesWithFile(node.sprite.atlasPath);
        ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.sprite.frameName);
        if (!frame)
            return false;

        sprite->setSpriteFrame(frame);
        prepareSpriteContentSize(*sprite, node);
        applySpriteRenderSettings(*sprite, node.sprite);
        return true;
    }

    if (node.sprite.imagePath.empty())
        return false;
    ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(node.sprite.imagePath);
    if (!texture)
        return false;

    sprite->setTexture(texture);
    sprite->setTextureRect(ax::Rect(0.0f, 0.0f,
                                    static_cast<float>(texture->getPixelsWide()),
                                    static_cast<float>(texture->getPixelsHigh())));
    prepareSpriteContentSize(*sprite, node);
    applySpriteRenderSettings(*sprite, node.sprite);
    return true;
}
}  // namespace editor
