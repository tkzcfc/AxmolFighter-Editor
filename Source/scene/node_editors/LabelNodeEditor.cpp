#include "scene/node_editors/LabelNodeEditor.h"

#include "2d/Label.h"
#include "core/SignatureUtils.h"
#include "imgui.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"
#include "editor_properties/AssetReferenceEditorProperty.h"
#include "editor_properties/BoolEditorProperty.h"
#include "editor_properties/ColorEditorProperty.h"
#include "editor_properties/EnumEditorProperty.h"
#include "editor_properties/FloatEditorProperty.h"
#include "editor_properties/MultilineStringEditorProperty.h"
#include "editor_properties/StringEditorProperty.h"
#include "editor_properties/Vec2EditorProperty.h"
#include "misc/cpp/imgui_stdlib.h"
#include "scene/SceneDocument.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace editor
{
namespace
{
ax::TextHAlignment toHorizontalAlignment(const std::string& value)
{
    if (value == "Right")
        return ax::TextHAlignment::RIGHT;
    if (value == "Center")
        return ax::TextHAlignment::CENTER;
    return ax::TextHAlignment::LEFT;
}

ax::TextVAlignment toVerticalAlignment(const std::string& value)
{
    if (value == "Bottom")
        return ax::TextVAlignment::BOTTOM;
    if (value == "Center")
        return ax::TextVAlignment::CENTER;
    return ax::TextVAlignment::TOP;
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

ax::Color4B toColor4B(const SceneColor& color)
{
    return ax::Color4B(colorByte(color.r), colorByte(color.g), colorByte(color.b), colorByte(color.a));
}

bool isBitmapFont(const SceneNode& node)
{
    return node.label.fontType == "BMFont";
}

bool isTrueTypeFont(const SceneNode& node)
{
    return node.label.fontType == "TTF";
}

ax::Label::Overflow toLabelOverflow(const std::string& mode)
{
    if (mode == "Height")
        return ax::Label::Overflow::RESIZE_HEIGHT;
    if (mode == "Shrink")
        return ax::Label::Overflow::SHRINK;
    if (mode == "None")
        return ax::Label::Overflow::NONE;
    return ax::Label::Overflow::NONE;
}

void configureLabelLayout(ax::Label& label, const SceneNode& node)
{
    const float width = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);
    if (node.label.autoSizeMode == "Height")
    {
        label.setDimensions(width, 0.0f);
        label.setOverflow(ax::Label::Overflow::RESIZE_HEIGHT);
    }
    else if (node.label.autoSizeMode == "Shrink")
    {
        label.setDimensions(width, height);
        label.setOverflow(ax::Label::Overflow::SHRINK);
    }
    else if (node.label.autoSizeMode == "Both")
    {
        label.setDimensions(0.0f, 0.0f);
        label.setOverflow(ax::Label::Overflow::NONE);
    }
    else
    {
        label.setDimensions(width, height);
        label.setOverflow(ax::Label::Overflow::CLAMP);
    }
}

ax::Vec2 labelMeasureDimensions(const SceneNode& node)
{
    const float width = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);
    if (node.label.autoSizeMode == "Both")
        return ax::Vec2::ZERO;
    if (node.label.autoSizeMode == "Height")
        return ax::Vec2(width, 0.0f);
    return ax::Vec2(width, height);
}

int labelMeasureLineWidth(const SceneNode& node)
{
    if (node.label.autoSizeMode == "Both")
        return 0;
    return static_cast<int>(std::max(1.0f, node.size.width));
}

ax::Label* createPreviewLabel(const SceneNode& node)
{
    const ax::TextHAlignment hAlignment = toHorizontalAlignment(node.label.horizontalAlignment);
    const ax::TextVAlignment vAlignment = toVerticalAlignment(node.label.verticalAlignment);
    const ax::Vec2 dimensions = labelMeasureDimensions(node);
    if (isBitmapFont(node))
    {
        if (node.label.fontPath.empty())
            return nullptr;
        return ax::Label::createWithBMFont(node.label.fontPath, node.label.text, hAlignment, labelMeasureLineWidth(node));
    }

    if (isTrueTypeFont(node))
    {
        if (node.label.fontPath.empty())
            return nullptr;
        return ax::Label::createWithTTF(node.label.text,
                                        node.label.fontPath,
                                        std::max(1.0f, node.label.fontSize),
                                        dimensions,
                                        hAlignment,
                                        vAlignment);
    }

    const std::string fontName = node.label.fontName.empty() ? "Arial" : node.label.fontName;
    return ax::Label::createWithSystemFont(node.label.text,
                                           fontName,
                                           std::max(1.0f, node.label.fontSize),
                                           dimensions,
                                           hAlignment,
                                           vAlignment);
}

bool applyLabelAutoSize(SceneNode& node)
{
    if (node.label.autoSizeMode == "None" || node.label.autoSizeMode == "Shrink")
        return false;

    ax::Label* label = createPreviewLabel(node);
    if (!label)
        return false;

    configureLabelLayout(*label, node);
    const ax::Vec2 contentSize = label->getContentSize();
    SceneSize nextSize = node.size;
    if (node.label.autoSizeMode == "Both")
    {
        nextSize.width = std::max(1.0f, contentSize.x);
        nextSize.height = std::max(1.0f, contentSize.y);
    }
    else if (node.label.autoSizeMode == "Height")
    {
        nextSize.height = std::max(1.0f, contentSize.y);
    }

    if (std::abs(nextSize.width - node.size.width) < 0.01f && std::abs(nextSize.height - node.size.height) < 0.01f)
        return false;

    node.size = nextSize;
    return true;
}

bool editLabelText(SceneDocument& document, SceneNode& node, std::string& activeEditKey, const std::string& key)
{
    ImGui::TextUnformatted("Text");
    bool prepared = false;
    if (!activeEditKey.empty() && !document.hasActiveUndoTransaction())
        activeEditKey.clear();
    if (activeEditKey.empty())
        prepared = document.beginUndoTransaction();

    const bool changed =
        ImGui::InputTextMultiline(("##" + key).c_str(), &node.label.text, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f));
    if (changed)
    {
        applyLabelAutoSize(node);
        document.markDirty();
    }

    if ((ImGui::IsItemActivated() || changed) && activeEditKey.empty())
        activeEditKey = key;

    const bool ownsEdit = activeEditKey == key;
    if (ownsEdit && (ImGui::IsItemDeactivated() || (changed && !ImGui::IsItemActive())))
    {
        if (applyLabelAutoSize(node))
            document.markDirty();
        document.commitUndoTransaction();
        activeEditKey.clear();
        return changed;
    }

    if (prepared && activeEditKey.empty())
        document.cancelUndoTransaction();
    return changed;
}

void applyLabelAutoSizeTransaction(SceneDocument& document, SceneNode& node)
{
    if (node.label.autoSizeMode == "None" || node.label.autoSizeMode == "Shrink")
        return;

    document.beginUndoTransaction();
    if (applyLabelAutoSize(node))
        document.markDirty();
    document.commitUndoTransaction();
}

bool editFontType(SceneDocument& document, SceneNode& node)
{
    const char* fontTypes[] = {"System", "TTF", "BMFont"};
    int selected = 0;
    for (int index = 0; index < IM_ARRAYSIZE(fontTypes); ++index)
    {
        if (node.label.fontType == fontTypes[index])
        {
            selected = index;
            break;
        }
    }

    if (!ImGui::Combo("Font Type", &selected, fontTypes, IM_ARRAYSIZE(fontTypes)))
        return false;

    document.beginUndoTransaction();
    node.label.fontType = fontTypes[selected];
    if (node.label.fontType == "BMFont")
        node.label.outlineEnabled = false;
    applyLabelAutoSize(node);
    document.markDirty();
    document.commitUndoTransaction();
    return true;
}

bool editLabelAutoSizeMode(SceneDocument& document, SceneNode& node)
{
    const char* autoSizeModes[] = {"None", "Both", "Height", "Shrink"};
    int selected = 0;
    for (int index = 0; index < IM_ARRAYSIZE(autoSizeModes); ++index)
    {
        if (node.label.autoSizeMode == autoSizeModes[index])
        {
            selected = index;
            break;
        }
    }

    if (!ImGui::Combo("Auto Size", &selected, autoSizeModes, IM_ARRAYSIZE(autoSizeModes)))
        return false;

    document.beginUndoTransaction();
    node.label.autoSizeMode = autoSizeModes[selected];
    applyLabelAutoSize(node);
    document.markDirty();
    document.commitUndoTransaction();
    return true;
}
}  // namespace

std::string_view LabelNodeEditor::typeId() const
{
    return "Label";
}

std::string_view LabelNodeEditor::displayName() const
{
    return "Label";
}

std::string_view LabelNodeEditor::defaultName() const
{
    return "Label";
}

void LabelNodeEditor::initializeNode(SceneNode& node) const
{
    BaseNodeEditor::initializeNode(node);
    node.type = std::string(typeId());
    node.name = std::string(defaultName());
    node.size = {180.0f, 48.0f};
    node.label.text = "Label";
}

void LabelNodeEditor::readCustomData(SceneNode& node, const rapidjson::Value& value) const
{
    if (!value.HasMember("label") || !value["label"].IsObject())
        return;

    const rapidjson::Value& label = value["label"];
    node.label.text = jsonStringOr(label, "text", node.label.text);
    node.label.fontType = jsonStringOr(label, "fontType", node.label.fontType);
    node.label.fontName = jsonStringOr(label, "fontName", node.label.fontName);
    node.label.fontPath = jsonStringOr(label, "fontPath", node.label.fontPath);
    node.label.fontSize = jsonNumberOr(label, "fontSize", node.label.fontSize);
    node.label.autoSizeMode = jsonStringOr(label, "autoSizeMode", node.label.autoSizeMode);
    node.label.horizontalAlignment = jsonStringOr(label, "horizontalAlignment", node.label.horizontalAlignment);
    node.label.verticalAlignment = jsonStringOr(label, "verticalAlignment", node.label.verticalAlignment);
    node.label.color = jsonColorOr(label, "color", node.label.color);
    node.label.outlineEnabled = jsonBoolOr(label, "outlineEnabled", node.label.outlineEnabled);
    node.label.outlineSize = jsonNumberOr(label, "outlineSize", node.label.outlineSize);
    node.label.outlineColor = jsonColorOr(label, "outlineColor", node.label.outlineColor);
    node.label.shadowEnabled = jsonBoolOr(label, "shadowEnabled", node.label.shadowEnabled);
    node.label.shadowOffset = {jsonNumberOr(label, "shadowOffsetX", node.label.shadowOffset.x),
                               jsonNumberOr(label, "shadowOffsetY", node.label.shadowOffset.y)};
    node.label.shadowColor = jsonColorOr(label, "shadowColor", node.label.shadowColor);
    if (isBitmapFont(node))
        node.label.outlineEnabled = false;
}

void LabelNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                           const SceneNode& node) const
{
    writer.Key("label");
    writer.StartObject();
    writer.Key("text");
    writer.String(node.label.text.c_str());
    writer.Key("fontType");
    writer.String(node.label.fontType.c_str());
    writer.Key("fontName");
    writer.String(node.label.fontName.c_str());
    writer.Key("fontPath");
    writer.String(node.label.fontPath.c_str());
    writer.Key("fontSize");
    writer.Double(node.label.fontSize);
    writer.Key("autoSizeMode");
    writer.String(node.label.autoSizeMode.c_str());
    writer.Key("horizontalAlignment");
    writer.String(node.label.horizontalAlignment.c_str());
    writer.Key("verticalAlignment");
    writer.String(node.label.verticalAlignment.c_str());
    writer.Key("color");
    writeColor(writer, node.label.color);
    writer.Key("outlineEnabled");
    writer.Bool(node.label.outlineEnabled);
    writer.Key("outlineSize");
    writer.Double(node.label.outlineSize);
    writer.Key("outlineColor");
    writeColor(writer, node.label.outlineColor);
    writer.Key("shadowEnabled");
    writer.Bool(node.label.shadowEnabled);
    writer.Key("shadowOffsetX");
    writer.Double(node.label.shadowOffset.x);
    writer.Key("shadowOffsetY");
    writer.Double(node.label.shadowOffset.y);
    writer.Key("shadowColor");
    writeColor(writer, node.label.shadowColor);
    writer.EndObject();
}

void LabelNodeEditor::appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const
{
    BaseNodeEditor::appendPropertyGroups(node, groups);

    const std::string editPrefix = node.id + ":label.";
    EditorPropertyGroup group;
    group.label = "Label";

    auto applyAutoSize = [](EditorPropertyContext& context) {
        applyLabelAutoSizeTransaction(context.document, context.node);
    };
    auto applyAutoSizeInline = [](EditorPropertyContext& context) {
        applyLabelAutoSize(context.node);
    };

    group.properties.push_back(std::make_unique<MultilineStringEditorProperty>(
        editPrefix + "text", "Text", node.label.text, applyAutoSizeInline, applyAutoSize));

    group.properties.push_back(std::make_unique<EnumEditorProperty>(editPrefix + "fontType",
                                                                   "Font Type",
                                                                   node.label.fontType,
                                                                   std::vector<std::string>{"System", "TTF", "BMFont"},
                                                                   [](EditorPropertyContext& context) {
                                                                       if (context.node.label.fontType == "BMFont")
                                                                           context.node.label.outlineEnabled = false;
                                                                       applyLabelAutoSize(context.node);
                                                                   }));

    if (isBitmapFont(node))
    {
        auto assignBitmapFont = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
            context.node.label.fontType = "BMFont";
            context.node.label.fontPath = asset.relativePath;
            context.node.label.outlineEnabled = false;
        };
        group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "fontPath",
                                                                                 "Font Path",
                                                                                 node.label.fontPath,
                                                                                 std::initializer_list<AssetKind>{AssetKind::BitmapFont},
                                                                                 assignBitmapFont,
                                                                                 applyAutoSize));
    }
    else if (isTrueTypeFont(node))
    {
        auto assignTrueTypeFont = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
            context.node.label.fontType = "TTF";
            context.node.label.fontPath = asset.relativePath;
        };
        group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "fontPath",
                                                                                 "Font Path",
                                                                                 node.label.fontPath,
                                                                                 std::initializer_list<AssetKind>{AssetKind::TrueTypeFont},
                                                                                 assignTrueTypeFont,
                                                                                 applyAutoSize));
    }
    else
    {
        group.properties.push_back(
            std::make_unique<StringEditorProperty>(editPrefix + "fontName", "Font Name", node.label.fontName, applyAutoSize));
    }

    group.properties.push_back(
        std::make_unique<FloatEditorProperty>(editPrefix + "fontSize", "Font Size", node.label.fontSize, applyAutoSize));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(editPrefix + "autoSizeMode",
                                                                   "Auto Size",
                                                                   node.label.autoSizeMode,
                                                                   std::vector<std::string>{"None", "Both", "Height", "Shrink"},
                                                                   [](EditorPropertyContext& context) {
                                                                       applyLabelAutoSize(context.node);
                                                                   }));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(editPrefix + "horizontalAlignment",
                                                                   "Horizontal Alignment",
                                                                   node.label.horizontalAlignment,
                                                                   std::vector<std::string>{"Left", "Center", "Right"}));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(editPrefix + "verticalAlignment",
                                                                   "Vertical Alignment",
                                                                   node.label.verticalAlignment,
                                                                   std::vector<std::string>{"Top", "Center", "Bottom"}));

    if (!isBitmapFont(node))
    {
        group.properties.push_back(
            std::make_unique<BoolEditorProperty>(editPrefix + "outlineEnabled", "Outline", node.label.outlineEnabled));
        if (node.label.outlineEnabled)
        {
            group.properties.push_back(
                std::make_unique<FloatEditorProperty>(editPrefix + "outlineSize", "Outline Size", node.label.outlineSize));
            group.properties.push_back(
                std::make_unique<ColorEditorProperty>(editPrefix + "outlineColor", "Outline Color", node.label.outlineColor));
        }
    }
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "shadowEnabled", "Shadow", node.label.shadowEnabled));
    if (node.label.shadowEnabled)
    {
        group.properties.push_back(
            std::make_unique<Vec2EditorProperty>(editPrefix + "shadowOffset", "Shadow Offset", node.label.shadowOffset));
        group.properties.push_back(
            std::make_unique<ColorEditorProperty>(editPrefix + "shadowColor", "Shadow Color", node.label.shadowColor));
    }
    groups.push_back(std::move(group));
}

SceneNodeSizeEditPolicy LabelNodeEditor::sizeEditPolicy(const SceneNode& node) const
{
    if (node.label.autoSizeMode == "Both")
        return {false, false, "Controlled by Label Auto Size"};
    if (node.label.autoSizeMode == "Height")
        return {true, false, "Height is controlled by Label Auto Size"};
    return {};
}

bool LabelNodeEditor::afterCanvasResize(SceneNode& node) const
{
    return applyLabelAutoSize(node);
}

std::tuple<std::string, ax::Node*> LabelNodeEditor::createEngineNode(const SceneNode& node,
                                                                      const SceneNodeRuntimeContext&) const
{
    const ax::Vec2 dimensions = labelMeasureDimensions(node);
    const ax::TextHAlignment hAlignment = toHorizontalAlignment(node.label.horizontalAlignment);
    const ax::TextVAlignment vAlignment = toVerticalAlignment(node.label.verticalAlignment);

    ax::Label* label = nullptr;
    if (isBitmapFont(node))
    {
        if (node.label.fontPath.empty())
            return {"BMFont path is empty", nullptr};
        label = ax::Label::createWithBMFont(node.label.fontPath, node.label.text, hAlignment, static_cast<int>(dimensions.x));
    }
    else if (isTrueTypeFont(node))
    {
        if (node.label.fontPath.empty())
            return {"TTF path is empty", nullptr};
        label = ax::Label::createWithTTF(
            node.label.text, node.label.fontPath, std::max(1.0f, node.label.fontSize), dimensions, hAlignment, vAlignment);
    }
    else
    {
        const std::string fontName = node.label.fontName.empty() ? "Arial" : node.label.fontName;
        label = ax::Label::createWithSystemFont(
            node.label.text, fontName, std::max(1.0f, node.label.fontSize), dimensions, hAlignment, vAlignment);
    }

    if (!label)
        return {"Failed to create label", nullptr};

    configureLabelLayout(*label, node);
    label->setAlignment(hAlignment, vAlignment);
    label->setTextColor(toColor4B(node.color));
    if (!isBitmapFont(node) && node.label.outlineEnabled)
        label->enableOutline(toColor4B(node.label.outlineColor), std::max(0.0f, node.label.outlineSize));
    if (node.label.shadowEnabled)
        label->enableShadow(toColor4B(node.label.shadowColor),
                            ax::Vec2(node.label.shadowOffset.x, node.label.shadowOffset.y),
                            0);
    return {"", label};
}

std::string LabelNodeEditor::engineSignature(const SceneNode& node, const SceneNodeRuntimeContext&) const
{
    return SignatureBuilder()
        .appendRaw("Label")
        .appendString("fontType", node.label.fontType)
        .appendString("fontName", node.label.fontName)
        .appendString("fontPath", node.label.fontPath)
        .appendFloat("fontSize", node.label.fontSize)
        .appendString("autoSizeMode", node.label.autoSizeMode)
        .hash();
}

bool LabelNodeEditor::updateEngineNode(ax::Node& runtimeNode,
                                            const SceneNode& node,
                                            const SceneNodeRuntimeContext&) const
{
    auto* label = dynamic_cast<ax::Label*>(&runtimeNode);
    if (!label)
        return false;

    const ax::TextHAlignment hAlignment = toHorizontalAlignment(node.label.horizontalAlignment);
    const ax::TextVAlignment vAlignment = toVerticalAlignment(node.label.verticalAlignment);
    label->setString("");
    configureLabelLayout(*label, node);
    label->setAlignment(hAlignment, vAlignment);
    label->setTextColor(toColor4B(node.color));

    if (!isBitmapFont(node) && node.label.outlineEnabled)
        label->enableOutline(toColor4B(node.label.outlineColor), std::max(0.0f, node.label.outlineSize));
    else
        label->disableEffect(ax::LabelEffect::OUTLINE);

    if (node.label.shadowEnabled)
        label->enableShadow(toColor4B(node.label.shadowColor),
                            ax::Vec2(node.label.shadowOffset.x, node.label.shadowOffset.y),
                            0);
    else
        label->disableEffect(ax::LabelEffect::SHADOW);

    label->setString(node.label.text);
    label->getContentSize();
    return true;
}
}  // namespace editor
