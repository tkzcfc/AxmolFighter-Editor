#include "scene/node_editors/FrameAnimationNodeEditor.h"

#include "2d/SpriteFrameCache.h"
#include "base/Director.h"
#include "core/SignatureUtils.h"
#include "documents/AniDocument.h"
#include "editor_properties/AssetReferenceEditorProperty.h"
#include "editor_properties/BoolEditorProperty.h"
#include "editor_properties/EnumEditorProperty.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "scene/node_editors/SceneNodeEditorUtils.h"
#include "scene/node_editors/SpriteRenderUtils.h"
#include "scene/runtime/FrameAnimationPreviewNode.h"
#include "platform/FileUtils.h"
#include "renderer/Texture2D.h"
#include "renderer/TextureCache.h"

#include <algorithm>
#include <sstream>

namespace editor
{
namespace
{
bool validPreviewVariableName(const std::string& name)
{
    return !name.empty() && name.find('{') == std::string::npos && name.find('}') == std::string::npos;
}

void writePreviewVariables(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                           const std::map<std::string, std::string>& variables)
{
    writer.Key("previewVars");
    writer.StartObject();
    for (const auto& [key, value] : variables)
    {
        writer.Key(key.c_str());
        writer.String(value.c_str());
    }
    writer.EndObject();
}

std::string blendDefaultCacheKey(const SceneNode& node)
{
    std::ostringstream key;
    key << node.frameAnimation.aniPath;
    for (const auto& [name, value] : node.frameAnimation.previewVars)
        key << '\x1f' << name << '=' << value;
    return key.str();
}

ax::Texture2D* firstFrameTexture(const SceneNode& node)
{
    if (node.frameAnimation.aniPath.empty())
        return nullptr;

    ax::FileUtils* fileUtils = ax::FileUtils::getInstance();
    const std::string resolvedAniPath = fileUtils->fullPathForFilename(node.frameAnimation.aniPath);
    if (resolvedAniPath.empty() || !fileUtils->isFileExist(resolvedAniPath))
        return nullptr;

    AniDocument document;
    if (!document.open(resolvedAniPath) || document.animation().frames.empty())
        return nullptr;

    AnimationPreviewVariables variables = document.animation().previewVars;
    for (const auto& [key, value] : node.frameAnimation.previewVars)
        variables[key] = value;

    const AnimationFrameData& frame = document.animation().frames.front();
    std::string imagePath;
    std::string error;
    if (frame.sourceType == AnimationFrameSourceType::Template)
    {
        if (!document.resolveTemplatePath(frame.templatePath, variables, imagePath, error))
            return nullptr;
    }
    else if (frame.sourceType == AnimationFrameSourceType::SpriteFrame)
    {
        const std::string atlasPath = fileUtils->fullPathForFilename(frame.atlasPath);
        if (atlasPath.empty() || !fileUtils->isFileExist(atlasPath))
            return nullptr;
        auto* cache = ax::SpriteFrameCache::getInstance();
        if (!cache->isSpriteFramesWithFileLoaded(atlasPath))
            cache->addSpriteFramesWithFile(atlasPath);
        auto* spriteFrame = cache->getSpriteFrameByName(frame.frameName);
        return spriteFrame ? spriteFrame->getTexture() : nullptr;
    }
    else
    {
        imagePath = frame.path;
    }

    const std::string texturePath = fileUtils->fullPathForFilename(imagePath);
    if (texturePath.empty() || !fileUtils->isFileExist(texturePath))
        return nullptr;
    return ax::Director::getInstance()->getTextureCache()->addImage(texturePath);
}
}  // namespace

std::string_view FrameAnimationNodeEditor::typeId() const
{
    return "FrameAnimation";
}

std::string_view FrameAnimationNodeEditor::displayName() const
{
    return "Frame Animation";
}

std::string_view FrameAnimationNodeEditor::defaultName() const
{
    return "Frame Animation";
}

void FrameAnimationNodeEditor::initializeNode(SceneNode& node) const
{
    BaseNodeEditor::initializeNode(node);
    node.type           = std::string(typeId());
    node.name           = std::string(defaultName());
    node.size           = {100.0f, 100.0f};
    node.frameAnimation = {};
}

void FrameAnimationNodeEditor::readCustomData(SceneNode& node, const rapidjson::Value& value) const
{
    if (!value.HasMember("frameAnimation") || !value["frameAnimation"].IsObject())
        return;

    const rapidjson::Value& data = value["frameAnimation"];
    node.frameAnimation.aniPath  = jsonStringOr(data, "aniPath", node.frameAnimation.aniPath);
    node.frameAnimation.blendSrc = normalizeSpriteBlendFactor(jsonStringOr(data, "blendSrc", node.frameAnimation.blendSrc), "");
    node.frameAnimation.blendDst = normalizeSpriteBlendFactor(jsonStringOr(data, "blendDst", node.frameAnimation.blendDst), "");
    node.frameAnimation.playing  = jsonBoolOr(data, "playing", node.frameAnimation.playing);
    node.frameAnimation.loop     = jsonBoolOr(data, "loop", node.frameAnimation.loop);
    if (data.HasMember("previewVars") && data["previewVars"].IsObject())
    {
        node.frameAnimation.previewVars.clear();
        for (auto it = data["previewVars"].MemberBegin(); it != data["previewVars"].MemberEnd(); ++it)
        {
            if (it->name.IsString() && it->value.IsString() && validPreviewVariableName(it->name.GetString()))
                node.frameAnimation.previewVars[it->name.GetString()] = it->value.GetString();
        }
    }
}

void FrameAnimationNodeEditor::writeCustomData(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                               const SceneNode& node) const
{
    writer.Key("frameAnimation");
    writer.StartObject();
    writer.Key("aniPath");
    writer.String(node.frameAnimation.aniPath.c_str());
    writer.Key("blendSrc");
    writer.String(normalizeSpriteBlendFactor(node.frameAnimation.blendSrc, "").c_str());
    writer.Key("blendDst");
    writer.String(normalizeSpriteBlendFactor(node.frameAnimation.blendDst, "").c_str());
    writer.Key("playing");
    writer.Bool(node.frameAnimation.playing);
    writer.Key("loop");
    writer.Bool(node.frameAnimation.loop);
    writePreviewVariables(writer, node.frameAnimation.previewVars);
    writer.EndObject();
}

void FrameAnimationNodeEditor::appendPropertyGroups(SceneNode& node, std::vector<EditorPropertyGroup>& groups) const
{
    BaseNodeEditor::appendPropertyGroups(node, groups);

    const std::string prefix = node.id + ":frameAnimation.";
    EditorPropertyGroup group;
    group.label    = "Frame Animation";
    const std::string blendSrcDefault = defaultBlendSrc(node);
    auto assignAni = [](EditorPropertyContext& context, const AssetDragPayload& asset) {
        if (asset.kind == AssetKind::FrameAnimation)
            context.node.frameAnimation.aniPath = asset.relativePath;
    };
    group.properties.push_back(std::make_unique<AssetReferenceEditorProperty>(
        prefix + "aniPath", "ANI Path", node.frameAnimation.aniPath,
        std::initializer_list<AssetKind>{AssetKind::FrameAnimation}, assignAni));
    group.properties.push_back(
        std::make_unique<BoolEditorProperty>(prefix + "playing", "Playing", node.frameAnimation.playing));
    group.properties.push_back(std::make_unique<BoolEditorProperty>(prefix + "loop", "Loop", node.frameAnimation.loop));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(prefix + "blendSrc",
                                                                    "Blend Src",
                                                                    node.frameAnimation.blendSrc,
                                                                    spriteBlendFactorNames(),
                                                                    blendSrcDefault));
    group.properties.push_back(std::make_unique<EnumEditorProperty>(prefix + "blendDst",
                                                                    "Blend Dst",
                                                                    node.frameAnimation.blendDst,
                                                                    spriteBlendFactorNames(),
                                                                    "ONE_MINUS_SRC_ALPHA"));
    groups.push_back(std::move(group));
}

std::string FrameAnimationNodeEditor::defaultBlendSrc(const SceneNode& node) const
{
    const std::string key = blendDefaultCacheKey(node);
    if (key == m_blendDefaultCacheKey)
        return m_blendDefaultSrc;

    m_blendDefaultCacheKey = key;
    ax::Texture2D* texture = firstFrameTexture(node);
    m_blendDefaultSrc = texture && texture->hasPremultipliedAlpha() ? "ONE" : "SRC_ALPHA";
    return m_blendDefaultSrc;
}

void FrameAnimationNodeEditor::drawInspector(SceneDocument& document, SceneNode& node, std::string& activeEditKey) const
{
    BaseNodeEditor::drawInspector(document, node, activeEditKey);

    ImGui::Separator();
    ImGui::TextUnformatted("Preview Variables");
    ImGui::Separator();
    if (ImGui::Button("Add Variable"))
    {
        document.beginUndoTransaction();
        std::string key = "var";
        int serial      = 1;
        while (node.frameAnimation.previewVars.contains(key))
            key = "var" + std::to_string(serial++);
        node.frameAnimation.previewVars.emplace(key, "");
        document.markDirty();
        document.commitUndoTransaction();
        m_previewVariableError.clear();
    }

    for (auto it = node.frameAnimation.previewVars.begin(); it != node.frameAnimation.previewVars.end();)
    {
        const std::string oldKey = it->first;
        ImGui::PushID(oldKey.c_str());
        if (m_previewVariableKeyEdit != oldKey)
        {
            m_previewVariableKeyEdit   = oldKey;
            m_previewVariableKeyBuffer = oldKey;
        }

        bool keyChanged = ImGui::InputText("Key", &m_previewVariableKeyBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
        keyChanged |= ImGui::IsItemDeactivatedAfterEdit();
        if (keyChanged && m_previewVariableKeyBuffer != oldKey)
        {
            const std::string newKey = m_previewVariableKeyBuffer;
            if (!validPreviewVariableName(newKey))
                m_previewVariableError = "Variable name cannot be empty or contain braces.";
            else if (node.frameAnimation.previewVars.contains(newKey))
                m_previewVariableError = "Variable name already exists.";
            else
            {
                document.beginUndoTransaction();
                const std::string value = std::move(it->second);
                it                      = node.frameAnimation.previewVars.erase(it);
                it                      = node.frameAnimation.previewVars.emplace_hint(it, newKey, value);
                document.markDirty();
                document.commitUndoTransaction();
                m_previewVariableKeyEdit.clear();
                m_previewVariableError.clear();
                ImGui::PopID();
                continue;
            }
        }

        ImGui::SameLine();
        const std::string valueEditId = node.id + ":frameAnimation.previewVars." + oldKey + ".value";
        const bool valueChanged       = ImGui::InputText("Value", &it->second);
        const bool valueActivated     = ImGui::IsItemActivated();
        if (valueActivated && activeEditKey.empty())
        {
            document.beginUndoTransaction();
            activeEditKey = valueEditId;
        }
        if (valueChanged)
            document.markDirty();
        if (activeEditKey == valueEditId && ImGui::IsItemDeactivated())
        {
            document.commitUndoTransaction();
            activeEditKey.clear();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            document.beginUndoTransaction();
            it = node.frameAnimation.previewVars.erase(it);
            document.markDirty();
            document.commitUndoTransaction();
            m_previewVariableKeyEdit.clear();
            m_previewVariableError.clear();
            ImGui::PopID();
            continue;
        }
        ImGui::PopID();
        ++it;
    }

    if (!m_previewVariableError.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", m_previewVariableError.c_str());
}

std::tuple<std::string, ax::Node*> FrameAnimationNodeEditor::createEngineNode(const SceneNode& node,
                                                                              const SceneNodeRuntimeContext&) const
{
    std::string error;
    auto* preview = FrameAnimationPreviewNode::create(node.frameAnimation.aniPath, node.frameAnimation.previewVars,
                                                      node.frameAnimation.loop, node.frameAnimation.playing,
                                                      node.frameAnimation.blendSrc, node.frameAnimation.blendDst, error);
    return {error, preview};
}

std::string FrameAnimationNodeEditor::engineSignature(const SceneNode& node, const SceneNodeRuntimeContext&) const
{
    SignatureBuilder builder;
    builder.appendRaw("FrameAnimation")
        .appendString("aniPath", node.frameAnimation.aniPath)
        .appendString("blendSrc", node.frameAnimation.blendSrc)
        .appendString("blendDst", node.frameAnimation.blendDst);
    return builder.hash();
}

bool FrameAnimationNodeEditor::updateEngineNode(ax::Node& runtimeNode,
                                                const SceneNode& node,
                                                const SceneNodeRuntimeContext&) const
{
    auto* preview = dynamic_cast<FrameAnimationPreviewNode*>(&runtimeNode);
    if (!preview)
        return false;
    std::string error;
    return preview->configure(node.frameAnimation.aniPath, node.frameAnimation.previewVars, node.frameAnimation.loop,
                              node.frameAnimation.playing, node.frameAnimation.blendSrc, node.frameAnimation.blendDst,
                              error);
}
}  // namespace editor
