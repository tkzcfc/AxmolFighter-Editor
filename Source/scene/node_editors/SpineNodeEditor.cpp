#include "scene/node_editors/SpineNodeEditor.h"

#include "core/SignatureUtils.h"
#include "imgui.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"
#include "editor_properties/AssetReferenceEditorProperty.h"
#include "editor_properties/BoolEditorProperty.h"
#include "editor_properties/FloatEditorProperty.h"
#include "editor_properties/OptionalStringEnumEditorProperty.h"
#include "scene/SceneDocument.h"
#include "spine/Animation.h"
#include "spine/SafeSkeletonAnimation.h"
#include "spine/SkeletonAnimation.h"
#include "spine/SkeletonData.h"
#include "spine/Skin.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace editor
{
namespace
{
struct SpinePreviewInfo
{
    std::vector<std::string> animations;
    std::vector<std::string> skins;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
};

SpinePreviewInfo loadSpinePreviewInfo(const SceneNode& node)
{
    SpinePreviewInfo info;
    if (node.spine.jsonPath.empty() || node.spine.atlasPath.empty())
        return info;

    auto [error, animation] = createSafeSkeletonAnimation(node.spine.jsonPath, node.spine.atlasPath);
    (void)error;
    if (!animation || !animation->getSkeleton() || !animation->getSkeleton()->getData())
        return info;

    spine::SkeletonData* data = animation->getSkeleton()->getData();
    spine::Vector<spine::Animation*>& animations = data->getAnimations();
    for (size_t index = 0; index < animations.size(); ++index)
    {
        if (animations[index])
            info.animations.emplace_back(animations[index]->getName().buffer());
    }

    spine::Vector<spine::Skin*>& skins = data->getSkins();
    for (size_t index = 0; index < skins.size(); ++index)
    {
        if (skins[index])
            info.skins.emplace_back(skins[index]->getName().buffer());
    }

    const ax::Rect bounds = animation->getBoundingBox();
    info.width = std::max(0.0f, bounds.size.width);
    info.height = std::max(0.0f, bounds.size.height);
    info.valid = true;
    return info;
}

bool containsValue(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool drawStringComboWithNone(SceneDocument& document,
                             const char* label,
                             std::string& value,
                             const std::vector<std::string>& values)
{
    const std::string preview = value.empty() ? "None" : value;
    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str()))
    {
        const bool selectedNone = value.empty();
        if (ImGui::Selectable("None", selectedNone))
        {
            document.beginUndoTransaction();
            value.clear();
            document.markDirty();
            document.commitUndoTransaction();
            changed = true;
        }
        for (const std::string& item : values)
        {
            const bool selected = value == item;
            if (ImGui::Selectable(item.c_str(), selected))
            {
                document.beginUndoTransaction();
                value = item;
                document.markDirty();
                document.commitUndoTransaction();
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool applySpineAutoSize(SceneNode& node, const SpinePreviewInfo& info)
{
    if (!node.spine.autoSize || !info.valid || info.width <= 0.0f || info.height <= 0.0f)
        return false;

    if (std::abs(node.size.width - info.width) < 0.01f && std::abs(node.size.height - info.height) < 0.01f)
        return false;

    node.size = {info.width, info.height};
    return true;
}

void applySpineAutoSizeTransaction(SceneDocument& document, SceneNode& node, const SpinePreviewInfo& info)
{
    if (!node.spine.autoSize)
        return;

    document.beginUndoTransaction();
    if (applySpineAutoSize(node, info))
        document.markDirty();
    document.commitUndoTransaction();
}
}  // namespace

std::string_view SpineNodeEditor::typeId() const
{
    return "Spine";
}

std::string_view SpineNodeEditor::displayName() const
{
    return "Spine";
}

std::string_view SpineNodeEditor::defaultName() const
{
    return "Spine";
}

void SpineNodeEditor::initializeNode(SceneNode& node) const
{
    BaseNodeEditor::initializeNode(node);
    node.type = std::string(typeId());
    node.name = std::string(defaultName());
    node.size = {160.0f, 160.0f};
}

void SpineNodeEditor::readCustomData(SceneNode& node, const rapidjson::Value& value) const
{
    if (!value.HasMember("spine") || !value["spine"].IsObject())
        return;

    const rapidjson::Value& spine = value["spine"];
    node.spine.jsonPath = jsonStringOr(spine, "jsonPath", node.spine.jsonPath);
    node.spine.atlasPath = jsonStringOr(spine, "atlasPath", node.spine.atlasPath);
    node.spine.animationName = jsonStringOr(spine, "animationName", node.spine.animationName);
    node.spine.skinName = jsonStringOr(spine, "skinName", node.spine.skinName);
    node.spine.loop = jsonBoolOr(spine, "loop", node.spine.loop);
    node.spine.timeScale = jsonNumberOr(spine, "timeScale", node.spine.timeScale);
    node.spine.debugBones = jsonBoolOr(spine, "debugBones", node.spine.debugBones);
    node.spine.debugSlots = jsonBoolOr(spine, "debugSlots", node.spine.debugSlots);
    node.spine.debugMeshes = jsonBoolOr(spine, "debugMeshes", node.spine.debugMeshes);
    node.spine.autoSize = jsonBoolOr(spine, "autoSize", node.spine.autoSize);
}

void SpineNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                           const SceneNode& node) const
{
    writer.Key("spine");
    writer.StartObject();
    writer.Key("jsonPath");
    writer.String(node.spine.jsonPath.c_str());
    writer.Key("atlasPath");
    writer.String(node.spine.atlasPath.c_str());
    writer.Key("animationName");
    writer.String(node.spine.animationName.c_str());
    writer.Key("skinName");
    writer.String(node.spine.skinName.c_str());
    writer.Key("loop");
    writer.Bool(node.spine.loop);
    writer.Key("timeScale");
    writer.Double(node.spine.timeScale);
    writer.Key("debugBones");
    writer.Bool(node.spine.debugBones);
    writer.Key("debugSlots");
    writer.Bool(node.spine.debugSlots);
    writer.Key("debugMeshes");
    writer.Bool(node.spine.debugMeshes);
    writer.Key("autoSize");
    writer.Bool(node.spine.autoSize);
    writer.EndObject();
}

void SpineNodeEditor::appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const
{
    BaseNodeEditor::appendPropertyGroups(node, groups);

    const std::string editPrefix = node.id + ":spine.";
    SpinePreviewInfo previewInfo = loadSpinePreviewInfo(node);
    EditorPropertyGroup group;
    group.label = "Spine";

    auto assignSpine = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
        context.node.spine.jsonPath = asset.relativePath;
        context.node.spine.atlasPath = asset.atlasPath;
        context.node.spine.animationName.clear();
        context.node.spine.skinName.clear();
        applySpineAutoSize(context.node, loadSpinePreviewInfo(context.node));
    };
    group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "jsonPath",
                                                                             "JSON Path",
                                                                             node.spine.jsonPath,
                                                                             std::initializer_list<AssetKind>{AssetKind::Spine},
                                                                             assignSpine));
    group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(editPrefix + "atlasPath",
                                                                             "Atlas Path",
                                                                             node.spine.atlasPath,
                                                                             std::initializer_list<AssetKind>{AssetKind::Spine},
                                                                             assignSpine));
    group.properties.push_back(std::make_unique<OptionalStringEnumEditorProperty>(editPrefix + "animationName",
                                                                                 "Animation",
                                                                                 node.spine.animationName,
                                                                                 previewInfo.animations));
    group.properties.push_back(
        std::make_unique<OptionalStringEnumEditorProperty>(editPrefix + "skinName", "Skin", node.spine.skinName, previewInfo.skins));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "loop", "Loop", node.spine.loop));
    group.properties.push_back(std::make_unique<FloatEditorProperty>(editPrefix + "timeScale", "Time Scale", node.spine.timeScale));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "autoSize",
                                                                   "Auto Size",
                                                                   node.spine.autoSize,
                                                                   true,
                                                                   [](EditorPropertyContext& context) {
                                                                       applySpineAutoSize(context.node,
                                                                                          loadSpinePreviewInfo(context.node));
                                                                   }));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "debugBones", "Debug Bones", node.spine.debugBones));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "debugSlots", "Debug Slots", node.spine.debugSlots));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(editPrefix + "debugMeshes", "Debug Meshes", node.spine.debugMeshes));
    groups.push_back(std::move(group));
}

SceneNodeSizeEditPolicy SpineNodeEditor::sizeEditPolicy(const SceneNode& node) const
{
    if (node.spine.autoSize)
        return {false, false, "Controlled by Spine Auto Size"};
    return {};
}

std::tuple<std::string, ax::Node*> SpineNodeEditor::createEngineNode(const SceneNode& node,
                                                                          const SceneNodeRuntimeContext&) const
{
    auto [error, animation] = createSafeSkeletonAnimation(node.spine.jsonPath, node.spine.atlasPath);
    if (!animation)
        return {error.empty() ? ("Failed to create Spine: " + node.spine.jsonPath) : error, nullptr};

    if (!node.spine.skinName.empty() && animation->getSkeleton() && animation->getSkeleton()->getData() &&
        animation->getSkeleton()->getData()->findSkin(node.spine.skinName.c_str()))
    {
        animation->setSkin(node.spine.skinName);
    }

    if (!node.spine.animationName.empty())
    {
        if (animation->findAnimation(node.spine.animationName))
            animation->setAnimation(0, node.spine.animationName, node.spine.loop);
    }

    animation->setTimeScale(std::max(0.0f, node.spine.timeScale));
    animation->setDebugBonesEnabled(node.spine.debugBones);
    animation->setDebugSlotsEnabled(node.spine.debugSlots);
    animation->setDebugMeshesEnabled(node.spine.debugMeshes);
    return {"", animation};
}

std::string SpineNodeEditor::engineSignature(const SceneNode& node, const SceneNodeRuntimeContext&) const
{
    return SignatureBuilder()
        .appendRaw("Spine")
        .appendString("jsonPath", node.spine.jsonPath)
        .appendString("atlasPath", node.spine.atlasPath)
        .hash();
}

bool SpineNodeEditor::updateEngineNode(ax::Node& runtimeNode,
                                            const SceneNode& node,
                                            const SceneNodeRuntimeContext&) const
{
    auto* animation = dynamic_cast<spine::SkeletonAnimation*>(&runtimeNode);
    if (!animation)
        return false;

    if (!node.spine.skinName.empty() && animation->getSkeleton() && animation->getSkeleton()->getData() &&
        animation->getSkeleton()->getData()->findSkin(node.spine.skinName.c_str()))
    {
        animation->setSkin(node.spine.skinName);
    }
    else
    {
        animation->setSkin("");
    }

    animation->clearTracks();
    if (!node.spine.animationName.empty() && animation->findAnimation(node.spine.animationName))
        animation->setAnimation(0, node.spine.animationName, node.spine.loop);

    animation->setTimeScale(std::max(0.0f, node.spine.timeScale));
    animation->setDebugBonesEnabled(node.spine.debugBones);
    animation->setDebugSlotsEnabled(node.spine.debugSlots);
    animation->setDebugMeshesEnabled(node.spine.debugMeshes);
    return true;
}
}  // namespace editor
