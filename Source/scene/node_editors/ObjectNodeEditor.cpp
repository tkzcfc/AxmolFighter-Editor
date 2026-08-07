#include "scene/node_editors/ObjectNodeEditor.h"

#include "2d/DrawNode.h"
#include "2d/Label.h"
#include "2d/Node.h"
#include "2d/Sprite.h"
#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "core/EditorPreferencesService.h"
#include "core/SignatureUtils.h"
#include "editor_properties/AssetReferenceEditorProperty.h"
#include "editor_properties/BoolEditorProperty.h"
#include "editor_properties/ButtonEditorProperty.h"
#include "editor_properties/ColorEditorProperty.h"
#include "editor_properties/EditorProperty.h"
#include "editor_properties/EnumEditorProperty.h"
#include "editor_properties/FloatEditorProperty.h"
#include "editor_properties/MultilineStringEditorProperty.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"

#include <algorithm>
#include <cfloat>
#include <functional>

namespace editor
{
namespace
{
constexpr const char* kObjectSourceTexture = "Texture";
constexpr const char* kObjectSourceSpriteFrame = "SpriteFrame";
constexpr const char* kObjectValueInt = "Int";
constexpr const char* kObjectValueString = "String";
constexpr const char* kObjectValueFloat = "Float";
constexpr const char* kObjectPreviewNodeName = "__object_preview";
constexpr const char* kObjectNoteNodeName = "__object_note";
constexpr int kObjectPreviewZOrder = -100000;
constexpr int kObjectNoteZOrder = 100000;

std::string normalizePreviewSourceType(const std::string& value)
{
    return value == kObjectSourceSpriteFrame ? kObjectSourceSpriteFrame : kObjectSourceTexture;
}

std::string normalizeObjectValueType(const std::string& value)
{
    if (value == kObjectValueInt || value == kObjectValueFloat)
        return value;
    return kObjectValueString;
}

int jsonIntOr(const rapidjson::Value& object, const char* key, int fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsInt())
        return fallback;
    return object[key].GetInt();
}

float jsonFloatOr(const rapidjson::Value& object, const char* key, float fallback)
{
    if (!object.IsObject() || !object.HasMember(key) || !object[key].IsNumber())
        return fallback;
    return object[key].GetFloat();
}

void writeColor(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const SceneColor& color)
{
    writer.StartObject();
    writer.Key("r");
    writer.Double(color.r);
    writer.Key("g");
    writer.Double(color.g);
    writer.Key("b");
    writer.Double(color.b);
    writer.Key("a");
    writer.Double(color.a);
    writer.EndObject();
}

unsigned char colorByte(float value)
{
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

ax::Color3B toColor3B(const SceneColor& color)
{
    return ax::Color3B(colorByte(color.r), colorByte(color.g), colorByte(color.b));
}

ax::Color4B toColor4B(const SceneColor& color)
{
    return ax::Color4B(colorByte(color.r), colorByte(color.g), colorByte(color.b), colorByte(color.a));
}

ax::Color4F previewFillColor(const SceneNode& node)
{
    return ax::Color4F(std::clamp(node.color.r, 0.0f, 1.0f),
                       std::clamp(node.color.g, 0.0f, 1.0f),
                       std::clamp(node.color.b, 0.0f, 1.0f),
                       std::clamp(node.color.a, 0.0f, 1.0f) * (std::clamp(node.opacity, 0, 255) / 255.0f));
}

std::string objectNoteFontPath()
{
    return EditorPreferencesService().editorFontPath();
}

std::vector<std::string> previewSourceTypeNames()
{
    return {kObjectSourceTexture, kObjectSourceSpriteFrame};
}

std::vector<std::string> objectValueTypeNames()
{
    return {kObjectValueInt, kObjectValueString, kObjectValueFloat};
}

std::string objectPreviewKind(const ObjectNodeData& object)
{
    const std::string sourceType = normalizePreviewSourceType(object.previewSourceType);
    if (sourceType == kObjectSourceSpriteFrame)
    {
        return !object.previewAtlasPath.empty() && !object.previewFrameName.empty() ? kObjectSourceSpriteFrame : "None";
    }
    return !object.previewImagePath.empty() ? kObjectSourceTexture : "None";
}

void applyPreviewSourceSizeFromPayload(SceneNode& node, const AssetDragPayload& asset)
{
    const int width = asset.sourceWidth > 0 ? asset.sourceWidth : asset.width;
    const int height = asset.sourceHeight > 0 ? asset.sourceHeight : asset.height;
    if (width > 0 && height > 0)
        node.size = {static_cast<float>(width), static_cast<float>(height)};
}

void assignObjectPreviewAsset(SceneNode& node, const AssetDragPayload& asset)
{
    if (asset.kind == AssetKind::SpriteFrame)
    {
        node.object.previewSourceType = kObjectSourceSpriteFrame;
        node.object.previewAtlasPath = asset.atlasPath;
        node.object.previewFrameName = asset.frameName;
        node.object.previewImagePath.clear();
        applyPreviewSourceSizeFromPayload(node, asset);
        return;
    }

    if (asset.kind == AssetKind::Texture)
    {
        node.object.previewSourceType = kObjectSourceTexture;
        node.object.previewImagePath = asset.relativePath;
        node.object.previewAtlasPath.clear();
        node.object.previewFrameName.clear();
        applyPreviewSourceSizeFromPayload(node, asset);
    }
}

bool applyObjectPreviewCurrentSourceSize(SceneNode& node)
{
    const std::string previewKind = objectPreviewKind(node.object);
    if (previewKind == kObjectSourceSpriteFrame)
    {
        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.object.previewAtlasPath))
            cache->addSpriteFramesWithFile(node.object.previewAtlasPath);

        ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.object.previewFrameName);
        if (!frame)
            return false;

        const ax::Vec2 size = frame->getOriginalSize();
        if (size.x <= 0.0f || size.y <= 0.0f)
            return false;
        node.size = {size.x, size.y};
        return true;
    }

    if (previewKind == kObjectSourceTexture)
    {
        ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(node.object.previewImagePath);
        if (!texture)
            return false;
        node.size = {static_cast<float>(texture->getPixelsWide()), static_cast<float>(texture->getPixelsHigh())};
        return node.size.width > 0.0f && node.size.height > 0.0f;
    }

    return false;
}

void preparePreviewSpriteContentSize(ax::Sprite& sprite, const SceneNode& node)
{
    sprite.setContentSize(ax::Vec2(std::max(1.0f, node.size.width), std::max(1.0f, node.size.height)));
}

void applyPreviewNodeVisual(ax::Node& previewNode, const SceneNode& node)
{
    previewNode.setAnchorPoint(ax::Vec2(0.0f, 0.0f));
    previewNode.setPosition(ax::Vec2::ZERO);
    previewNode.setColor(toColor3B(node.color));
    previewNode.setOpacity(static_cast<uint8_t>(std::clamp(node.opacity, 0, 255)));
}

ax::DrawNode* createPlaceholderPreviewNode(const SceneNode& node)
{
    ax::DrawNode* drawNode = ax::DrawNode::create();
    if (!drawNode)
        return nullptr;

    const float width = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);
    drawNode->drawSolidRect(ax::Vec2(0.0f, 0.0f), ax::Vec2(width, height), previewFillColor(node));
    drawNode->setContentSize(ax::Vec2(width, height));
    applyPreviewNodeVisual(*drawNode, node);
    return drawNode;
}

std::tuple<std::string, ax::Node*> createPreviewNode(const SceneNode& node)
{
    const std::string previewKind = objectPreviewKind(node.object);
    if (previewKind == "None")
    {
        ax::DrawNode* drawNode = createPlaceholderPreviewNode(node);
        if (!drawNode)
            return {"Failed to create object placeholder", nullptr};
        return {"", drawNode};
    }

    if (previewKind == kObjectSourceSpriteFrame)
    {
        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.object.previewAtlasPath))
            cache->addSpriteFramesWithFile(node.object.previewAtlasPath);
        if (!cache->getSpriteFrameByName(node.object.previewFrameName))
            return {"Failed to load object preview frame: " + node.object.previewFrameName, nullptr};

        ax::Sprite* sprite = ax::Sprite::createWithSpriteFrameName(node.object.previewFrameName);
        if (!sprite || !sprite->getTexture())
            return {"Failed to create object preview frame: " + node.object.previewFrameName, nullptr};
        preparePreviewSpriteContentSize(*sprite, node);
        applyPreviewNodeVisual(*sprite, node);
        return {"", sprite};
    }

    if (previewKind == kObjectSourceTexture)
    {
        ax::Sprite* sprite = ax::Sprite::create(node.object.previewImagePath);
        if (!sprite || !sprite->getTexture())
            return {"Failed to load object preview image: " + node.object.previewImagePath, nullptr};
        preparePreviewSpriteContentSize(*sprite, node);
        applyPreviewNodeVisual(*sprite, node);
        return {"", sprite};
    }

    return {"", nullptr};
}

bool updatePlaceholderPreviewNode(ax::DrawNode& drawNode, const SceneNode& node)
{
    drawNode.clear();
    const float width = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);
    drawNode.drawSolidRect(ax::Vec2(0.0f, 0.0f), ax::Vec2(width, height), previewFillColor(node));
    drawNode.setContentSize(ax::Vec2(width, height));
    applyPreviewNodeVisual(drawNode, node);
    return true;
}

bool updatePreviewNode(ax::Node& previewNode, const SceneNode& node)
{
    const std::string previewKind = objectPreviewKind(node.object);
    if (previewKind == "None")
    {
        auto* drawNode = dynamic_cast<ax::DrawNode*>(&previewNode);
        return drawNode ? updatePlaceholderPreviewNode(*drawNode, node) : false;
    }

    auto* sprite = dynamic_cast<ax::Sprite*>(&previewNode);
    if (!sprite)
        return false;

    if (previewKind == kObjectSourceSpriteFrame)
    {
        ax::SpriteFrameCache* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(node.object.previewAtlasPath))
            cache->addSpriteFramesWithFile(node.object.previewAtlasPath);
        ax::SpriteFrame* frame = cache->getSpriteFrameByName(node.object.previewFrameName);
        if (!frame)
            return false;
        sprite->setSpriteFrame(frame);
        preparePreviewSpriteContentSize(*sprite, node);
        applyPreviewNodeVisual(*sprite, node);
        return true;
    }

    if (previewKind == kObjectSourceTexture)
    {
        ax::Texture2D* texture = ax::Director::getInstance()->getTextureCache()->addImage(node.object.previewImagePath);
        if (!texture)
            return false;
        sprite->setTexture(texture);
        sprite->setTextureRect(ax::Rect(0.0f,
                                        0.0f,
                                        static_cast<float>(texture->getPixelsWide()),
                                        static_cast<float>(texture->getPixelsHigh())));
        preparePreviewSpriteContentSize(*sprite, node);
        applyPreviewNodeVisual(*sprite, node);
        return true;
    }

    return false;
}

ax::Label* createNoteLabel(const SceneNode& node, const SceneNodeRuntimeContext& context)
{
    (void)context;
    const std::string fontPath = objectNoteFontPath();
    if (node.object.note.empty() || fontPath.empty())
        return nullptr;

    const ax::Vec2 dimensions(0.0f, 0.0f);
    ax::Label* label = ax::Label::createWithTTF(node.object.note,
                                                fontPath,
                                                std::max(1.0f, node.object.noteFontSize),
                                                dimensions,
                                                ax::TextHAlignment::CENTER,
                                                ax::TextVAlignment::TOP);
    if (!label)
        return nullptr;

    label->setName(kObjectNoteNodeName);
    label->setTextColor(toColor4B(node.object.noteColor));
    if (node.object.noteOutlineEnabled)
        label->enableOutline(toColor4B(node.object.noteOutlineColor), std::max(0.0f, node.object.noteOutlineSize));
    label->setAlignment(ax::TextHAlignment::CENTER, ax::TextVAlignment::TOP);
    label->setAnchorPoint(ax::Vec2(0.5f, 1.0f));
    label->setPosition(ax::Vec2(node.size.width * 0.5f, node.size.height));
    return label;
}

bool updateNoteLabel(ax::Node& container, const SceneNode& node, const SceneNodeRuntimeContext& context)
{
    ax::Node* existing = container.getChildByName(kObjectNoteNodeName);
    if (node.object.note.empty())
    {
        if (existing)
            existing->removeFromParentAndCleanup(true);
        return true;
    }

    auto* label = dynamic_cast<ax::Label*>(existing);
    if (!label)
    {
        if (existing)
            existing->removeFromParentAndCleanup(true);
        label = createNoteLabel(node, context);
        if (!label)
            return false;
        container.addChild(label, kObjectNoteZOrder, kObjectNoteNodeName);
        return true;
    }

    const std::string fontPath = objectNoteFontPath();
    if (fontPath.empty())
        return false;

    label->setString("");
    label->setTTFConfig({fontPath, std::max(1.0f, node.object.noteFontSize)});
    label->setDimensions(0.0f, 0.0f);
    label->setTextColor(toColor4B(node.object.noteColor));
    if (node.object.noteOutlineEnabled)
        label->enableOutline(toColor4B(node.object.noteOutlineColor), std::max(0.0f, node.object.noteOutlineSize));
    else
        label->disableEffect(ax::LabelEffect::OUTLINE);
    label->setAlignment(ax::TextHAlignment::CENTER, ax::TextVAlignment::TOP);
    label->setAnchorPoint(ax::Vec2(0.5f, 1.0f));
    label->setPosition(ax::Vec2(node.size.width * 0.5f, node.size.height));
    label->setString(node.object.note);
    return true;
}

std::tuple<std::string, ax::Node*> createObjectContainer(const SceneNode& node, const SceneNodeRuntimeContext& context)
{
    ax::Node* container = ax::Node::create();
    if (!container)
        return {"Failed to create object container", nullptr};

    auto [previewError, previewNode] = createPreviewNode(node);
    if (!previewNode)
        return {previewError.empty() ? "Failed to create object preview" : previewError, nullptr};
    previewNode->setName(kObjectPreviewNodeName);
    container->addChild(previewNode, kObjectPreviewZOrder, kObjectPreviewNodeName);

    if (!node.object.note.empty())
    {
        ax::Label* note = createNoteLabel(node, context);
        if (!note)
            return {"Failed to create object note label", nullptr};
        container->addChild(note, kObjectNoteZOrder, kObjectNoteNodeName);
    }

    return {"", container};
}

bool updateObjectContainer(ax::Node& container, const SceneNode& node, const SceneNodeRuntimeContext& context)
{
    ax::Node* previewNode = container.getChildByName(kObjectPreviewNodeName);
    if (!previewNode)
    {
        auto [previewError, createdPreview] = createPreviewNode(node);
        (void)previewError;
        if (!createdPreview)
            return false;
        createdPreview->setName(kObjectPreviewNodeName);
        container.addChild(createdPreview, kObjectPreviewZOrder, kObjectPreviewNodeName);
        previewNode = createdPreview;
    }

    return updatePreviewNode(*previewNode, node) && updateNoteLabel(container, node, context);
}

std::string nextObjectPropertyKey(const std::vector<ObjectKeyValue>& properties)
{
    const auto exists = [&](const std::string& key) {
        return std::any_of(properties.begin(), properties.end(), [&](const ObjectKeyValue& property) {
            return property.key == key;
        });
    };

    if (!exists("key"))
        return "key";

    for (int index = 1;; ++index)
    {
        const std::string candidate = "key_" + std::to_string(index);
        if (!exists(candidate))
            return candidate;
    }
}

void commitImmediateEdit(SceneDocument& document, const std::function<void()>& edit)
{
    document.beginUndoTransaction();
    edit();
    document.markDirty();
    document.commitUndoTransaction();
}

void drawObjectPropertiesTable(SceneDocument& document, SceneNode& node)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Object Properties");
    ImGui::Separator();

    if (ImGui::Button("Add Property"))
    {
        commitImmediateEdit(document, [&]() {
            ObjectKeyValue property;
            property.key = nextObjectPropertyKey(node.object.properties);
            node.object.properties.push_back(std::move(property));
        });
    }

    if (!ImGui::BeginTable("ObjectProperties", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        return;

    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.35f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableHeadersRow();

    int pendingDeleteIndex = -1;
    for (int index = 0; index < static_cast<int>(node.object.properties.size()); ++index)
    {
        ObjectKeyValue& property = node.object.properties[index];
        property.type = normalizeObjectValueType(property.type);

        ImGui::PushID(index);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        std::string key = property.key;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##key", &key) && key != property.key)
        {
            commitImmediateEdit(document, [&]() {
                property.key = key;
            });
        }

        ImGui::TableSetColumnIndex(1);
        std::string type = property.type;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##type", type.c_str()))
        {
            for (const std::string& item : objectValueTypeNames())
            {
                const bool selected = type == item;
                if (ImGui::Selectable(item.c_str(), selected))
                {
                    commitImmediateEdit(document, [&]() {
                        property.type = item;
                    });
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (property.type == kObjectValueInt)
        {
            int value = property.intValue;
            if (ImGui::InputInt("##value", &value) && value != property.intValue)
            {
                commitImmediateEdit(document, [&]() {
                    property.intValue = value;
                });
            }
        }
        else if (property.type == kObjectValueFloat)
        {
            float value = property.floatValue;
            if (ImGui::InputFloat("##value", &value) && value != property.floatValue)
            {
                commitImmediateEdit(document, [&]() {
                    property.floatValue = value;
                });
            }
        }
        else
        {
            std::string value = property.stringValue;
            if (ImGui::InputText("##value", &value) && value != property.stringValue)
            {
                commitImmediateEdit(document, [&]() {
                    property.stringValue = value;
                });
            }
        }

        ImGui::TableSetColumnIndex(3);
        if (ImGui::SmallButton("Delete"))
            pendingDeleteIndex = index;

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (pendingDeleteIndex >= 0 && pendingDeleteIndex < static_cast<int>(node.object.properties.size()))
    {
        commitImmediateEdit(document, [&]() {
            node.object.properties.erase(node.object.properties.begin() + pendingDeleteIndex);
        });
    }
}
}  // namespace

std::string_view ObjectNodeEditor::typeId() const
{
    return "Object";
}

std::string_view ObjectNodeEditor::displayName() const
{
    return "Object";
}

std::string_view ObjectNodeEditor::defaultName() const
{
    return "Object";
}

void ObjectNodeEditor::initializeNode(SceneNode& node) const
{
    BaseNodeEditor::initializeNode(node);
    node.anchor = {0.5f, 0.0f};

    const EditorPreferencesService preferences;
    node.object.noteColor = preferences.objectNoteColor();
    node.object.noteFontSize = preferences.objectNoteFontSize();
    node.object.noteOutlineEnabled = preferences.objectNoteOutlineEnabled();
    node.object.noteOutlineColor = preferences.objectNoteOutlineColor();
    node.object.noteOutlineSize = preferences.objectNoteOutlineSize();
}

void ObjectNodeEditor::readCustomData(SceneNode& node, const rapidjson::Value& value) const
{
    if (!value.HasMember("object") || !value["object"].IsObject())
        return;

    const rapidjson::Value& object = value["object"];
    node.object.previewSourceType =
        normalizePreviewSourceType(jsonStringOr(object, "previewSourceType", node.object.previewSourceType));
    node.object.previewImagePath = jsonStringOr(object, "previewImagePath", node.object.previewImagePath);
    node.object.previewAtlasPath = jsonStringOr(object, "previewAtlasPath", node.object.previewAtlasPath);
    node.object.previewFrameName = jsonStringOr(object, "previewFrameName", node.object.previewFrameName);
    node.object.note = jsonStringOr(object, "note", node.object.note);
    node.object.noteColor = jsonColorOr(object, "noteColor", node.object.noteColor);
    node.object.noteFontSize = jsonNumberOr(object, "noteFontSize", node.object.noteFontSize);
    node.object.noteOutlineEnabled = jsonBoolOr(object, "noteOutlineEnabled", node.object.noteOutlineEnabled);
    node.object.noteOutlineColor = jsonColorOr(object, "noteOutlineColor", node.object.noteOutlineColor);
    node.object.noteOutlineSize = jsonNumberOr(object, "noteOutlineSize", node.object.noteOutlineSize);
    node.object.properties.clear();

    if (!object.HasMember("properties") || !object["properties"].IsArray())
        return;

    for (const rapidjson::Value& propertyValue : object["properties"].GetArray())
    {
        if (!propertyValue.IsObject())
            continue;

        ObjectKeyValue property;
        property.key = jsonStringOr(propertyValue, "key", property.key);
        property.type = normalizeObjectValueType(jsonStringOr(propertyValue, "type", property.type));
        if (property.type == kObjectValueInt)
            property.intValue = jsonIntOr(propertyValue, "value", property.intValue);
        else if (property.type == kObjectValueFloat)
            property.floatValue = jsonFloatOr(propertyValue, "value", property.floatValue);
        else
            property.stringValue = jsonStringOr(propertyValue, "value", property.stringValue);
        node.object.properties.push_back(std::move(property));
    }
}

void ObjectNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                       const SceneNode& node) const
{
    writer.Key("object");
    writer.StartObject();
    writer.Key("previewSourceType");
    writer.String(normalizePreviewSourceType(node.object.previewSourceType).c_str());
    writer.Key("previewImagePath");
    writer.String(node.object.previewImagePath.c_str());
    writer.Key("previewAtlasPath");
    writer.String(node.object.previewAtlasPath.c_str());
    writer.Key("previewFrameName");
    writer.String(node.object.previewFrameName.c_str());
    writer.Key("note");
    writer.String(node.object.note.c_str());
    writer.Key("noteColor");
    writeColor(writer, node.object.noteColor);
    writer.Key("noteFontSize");
    writer.Double(node.object.noteFontSize);
    writer.Key("noteOutlineEnabled");
    writer.Bool(node.object.noteOutlineEnabled);
    writer.Key("noteOutlineColor");
    writeColor(writer, node.object.noteOutlineColor);
    writer.Key("noteOutlineSize");
    writer.Double(node.object.noteOutlineSize);
    writer.Key("properties");
    writer.StartArray();
    for (const ObjectKeyValue& property : node.object.properties)
    {
        const std::string type = normalizeObjectValueType(property.type);
        writer.StartObject();
        writer.Key("key");
        writer.String(property.key.c_str());
        writer.Key("type");
        writer.String(type.c_str());
        writer.Key("value");
        if (type == kObjectValueInt)
            writer.Int(property.intValue);
        else if (type == kObjectValueFloat)
            writer.Double(property.floatValue);
        else
            writer.String(property.stringValue.c_str());
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
}

void ObjectNodeEditor::drawInspector(SceneDocument& document, SceneNode& node, std::string& activeEditKey) const
{
    std::vector<EditorPropertyGroup> groups;
    BaseNodeEditor::appendPropertyGroups(node, groups);

    EditorPropertyGroup objectGroup;
    objectGroup.label = "Object";
    const std::string editPrefix = node.id + ":object.";
    objectGroup.properties.push_back(std::make_unique<EnumEditorProperty>(
        editPrefix + "previewSourceType",
        "Preview Source Type",
        node.object.previewSourceType,
        previewSourceTypeNames(),
        [](EditorPropertyContext& context) {
            context.node.object.previewSourceType = normalizePreviewSourceType(context.node.object.previewSourceType);
        }));

    auto assignPreview = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
        assignObjectPreviewAsset(context.node, asset);
    };

    if (normalizePreviewSourceType(node.object.previewSourceType) == kObjectSourceSpriteFrame)
    {
        objectGroup.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(
            editPrefix + "previewAtlasPath",
            "Preview Atlas",
            node.object.previewAtlasPath,
            std::initializer_list<AssetKind>{AssetKind::Texture, AssetKind::SpriteFrame},
            assignPreview));
        objectGroup.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(
            editPrefix + "previewFrameName",
            "Preview Frame",
            node.object.previewFrameName,
            std::initializer_list<AssetKind>{AssetKind::Texture, AssetKind::SpriteFrame},
            assignPreview));
    }
    else
    {
        objectGroup.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(
            editPrefix + "previewImagePath",
            "Preview Image",
            node.object.previewImagePath,
            std::initializer_list<AssetKind>{AssetKind::Texture, AssetKind::SpriteFrame},
            assignPreview));
    }

    objectGroup.properties.push_back(std::make_unique<ButtonEditorProperty>(
        editPrefix + "setSizeToPreview",
        "Set Size To Preview",
        [](EditorPropertyContext& context) {
            context.document.beginUndoTransaction();
            if (applyObjectPreviewCurrentSourceSize(context.node))
                context.document.markDirty();
            context.document.commitUndoTransaction();
        }));
    objectGroup.properties.push_back(
        std::make_unique<MultilineStringEditorProperty>(editPrefix + "note", "Note", node.object.note));
    objectGroup.properties.push_back(
        std::make_unique<ColorEditorProperty>(editPrefix + "noteColor", "Note Color", node.object.noteColor));
    objectGroup.properties.push_back(
        std::make_unique<FloatEditorProperty>(editPrefix + "noteFontSize", "Note Font Size", node.object.noteFontSize));
    objectGroup.properties.push_back(std::make_unique<BoolEditorProperty>(
        editPrefix + "noteOutlineEnabled", "Note Outline", node.object.noteOutlineEnabled));
    if (node.object.noteOutlineEnabled)
    {
        objectGroup.properties.push_back(std::make_unique<ColorEditorProperty>(
            editPrefix + "noteOutlineColor", "Note Outline Color", node.object.noteOutlineColor));
        objectGroup.properties.push_back(std::make_unique<FloatEditorProperty>(
            editPrefix + "noteOutlineSize", "Note Outline Size", node.object.noteOutlineSize));
    }

    groups.push_back(std::move(objectGroup));

    EditorPropertyContext context{nullptr, document, node, activeEditKey};
    drawEditorPropertyGroups(context, groups);
    drawObjectPropertiesTable(document, node);
}

std::tuple<std::string, ax::Node*> ObjectNodeEditor::createEngineNode(const SceneNode& node,
                                                                      const SceneNodeRuntimeContext& context) const
{
    return createObjectContainer(node, context);
}

bool ObjectNodeEditor::canHaveChildren() const
{
    return false;
}

std::string ObjectNodeEditor::engineSignature(const SceneNode& node, const SceneNodeRuntimeContext&) const
{
    SignatureBuilder builder;
    builder.appendRaw("Object")
        .appendString("previewKind", objectPreviewKind(node.object))
        .appendBool("hasNote", !node.object.note.empty());
    if (!node.object.note.empty())
    {
        builder.appendFloat("noteFontSize", node.object.noteFontSize);
    }
    return builder.hash();
}

bool ObjectNodeEditor::updateEngineNode(ax::Node& runtimeNode,
                                        const SceneNode& node,
                                        const SceneNodeRuntimeContext& context) const
{
    return updateObjectContainer(runtimeNode, node, context);
}
}  // namespace editor
