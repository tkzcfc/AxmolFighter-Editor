#include "scene/runtime/ScenePreviewRuntime.h"

#include "core/SignatureUtils.h"
#include "scene/node_editors/SceneNodeEditor.h"
#include "2d/DrawNode.h"
#include "2d/Label.h"
#include "2d/Node.h"
#include "2d/RenderTexture.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#ifndef AXMOL_FIGHTER_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE
#    define AXMOL_FIGHTER_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE 1
#endif

namespace editor
{
namespace
{
int roundUpToMultiple(int value, int multiple)
{
    if (multiple <= 0)
        return value;
    return ((std::max(1, value) + multiple - 1) / multiple) * multiple;
}

int expandedRenderTextureCapacity(int requested, int currentCapacity)
{
    constexpr int kCapacityGranularity = 256;
    if (requested <= currentCapacity)
        return currentCapacity;

    const int grownCapacity = currentCapacity > 0 ? static_cast<int>(std::ceil(currentCapacity * 1.5f)) : requested;
    return roundUpToMultiple(std::max(requested, grownCapacity), kCapacityGranularity);
}

std::string sceneNodeUpdateSignature(const SceneNode& node)
{
    SignatureBuilder builder;
    builder.appendRaw(node.type)
        .appendFloat("size.width", node.size.width)
        .appendFloat("size.height", node.size.height)
        .appendFloat("color.r", node.color.r)
        .appendFloat("color.g", node.color.g)
        .appendFloat("color.b", node.color.b)
        .appendFloat("color.a", node.color.a)
        .appendInt("opacity", node.opacity);

    if (node.type == "Node")
    {
        builder.appendBool("node.drawFill", node.node.drawFill);
    }
    else if (node.type == "Sprite")
    {
        builder.appendString("sprite.sourceType", node.sprite.sourceType)
            .appendString("sprite.imagePath", node.sprite.imagePath)
            .appendString("sprite.atlasPath", node.sprite.atlasPath)
            .appendString("sprite.frameName", node.sprite.frameName)
            .appendString("sprite.renderType", node.sprite.renderType)
            .appendString("sprite.blendSrc", node.sprite.blendSrc)
            .appendString("sprite.blendDst", node.sprite.blendDst)
            .appendFloat("sprite.sliceCenterOrigin.x", node.sprite.sliceCenterOrigin.x)
            .appendFloat("sprite.sliceCenterOrigin.y", node.sprite.sliceCenterOrigin.y)
            .appendFloat("sprite.sliceCenterSize.x", node.sprite.sliceCenterSize.x)
            .appendFloat("sprite.sliceCenterSize.y", node.sprite.sliceCenterSize.y);
    }
    else if (node.type == "Label")
    {
        builder.appendString("label.text", node.label.text)
            .appendString("label.autoSizeMode", node.label.autoSizeMode)
            .appendString("label.horizontalAlignment", node.label.horizontalAlignment)
            .appendString("label.verticalAlignment", node.label.verticalAlignment)
            .appendBool("label.outlineEnabled", node.label.outlineEnabled)
            .appendFloat("label.outlineSize", node.label.outlineSize)
            .appendFloat("label.outlineColor.r", node.label.outlineColor.r)
            .appendFloat("label.outlineColor.g", node.label.outlineColor.g)
            .appendFloat("label.outlineColor.b", node.label.outlineColor.b)
            .appendFloat("label.outlineColor.a", node.label.outlineColor.a)
            .appendBool("label.shadowEnabled", node.label.shadowEnabled)
            .appendFloat("label.shadowOffset.x", node.label.shadowOffset.x)
            .appendFloat("label.shadowOffset.y", node.label.shadowOffset.y)
            .appendFloat("label.shadowColor.r", node.label.shadowColor.r)
            .appendFloat("label.shadowColor.g", node.label.shadowColor.g)
            .appendFloat("label.shadowColor.b", node.label.shadowColor.b)
            .appendFloat("label.shadowColor.a", node.label.shadowColor.a);
    }
    else if (node.type == "Spine")
    {
        builder.appendString("spine.animationName", node.spine.animationName)
            .appendString("spine.skinName", node.spine.skinName)
            .appendBool("spine.loop", node.spine.loop)
            .appendFloat("spine.timeScale", node.spine.timeScale)
            .appendBool("spine.debugBones", node.spine.debugBones)
            .appendBool("spine.debugSlots", node.spine.debugSlots)
            .appendBool("spine.debugMeshes", node.spine.debugMeshes);
    }
    else if (node.type == "FrameAnimation")
    {
        builder.appendString("frameAnimation.aniPath", node.frameAnimation.aniPath)
            .appendString("frameAnimation.blendSrc", node.frameAnimation.blendSrc)
            .appendString("frameAnimation.blendDst", node.frameAnimation.blendDst)
            .appendBool("frameAnimation.playing", node.frameAnimation.playing)
            .appendBool("frameAnimation.loop", node.frameAnimation.loop)
            .appendInt("frameAnimation.previewVars.size", static_cast<int>(node.frameAnimation.previewVars.size()));
        for (const auto& [key, value] : node.frameAnimation.previewVars)
            builder.appendString("frameAnimation.previewVars." + key, value);
    }
    else if (node.type == "Object")
    {
        builder.appendString("object.previewSourceType", node.object.previewSourceType)
            .appendString("object.previewImagePath", node.object.previewImagePath)
            .appendString("object.previewAtlasPath", node.object.previewAtlasPath)
            .appendString("object.previewFrameName", node.object.previewFrameName)
            .appendString("object.note", node.object.note)
            .appendFloat("object.noteColor.r", node.object.noteColor.r)
            .appendFloat("object.noteColor.g", node.object.noteColor.g)
            .appendFloat("object.noteColor.b", node.object.noteColor.b)
            .appendFloat("object.noteColor.a", node.object.noteColor.a)
            .appendFloat("object.noteFontSize", node.object.noteFontSize)
            .appendBool("object.noteOutlineEnabled", node.object.noteOutlineEnabled)
            .appendFloat("object.noteOutlineColor.r", node.object.noteOutlineColor.r)
            .appendFloat("object.noteOutlineColor.g", node.object.noteOutlineColor.g)
            .appendFloat("object.noteOutlineColor.b", node.object.noteOutlineColor.b)
            .appendFloat("object.noteOutlineColor.a", node.object.noteOutlineColor.a)
            .appendFloat("object.noteOutlineSize", node.object.noteOutlineSize)
            .appendInt("object.properties.size", static_cast<int>(node.object.properties.size()));
        for (int index = 0; index < static_cast<int>(node.object.properties.size()); ++index)
        {
            const ObjectKeyValue& property = node.object.properties[index];
            const std::string prefix       = "object.properties." + std::to_string(index) + ".";
            builder.appendString(prefix + "key", property.key).appendString(prefix + "type", property.type);
            if (property.type == "Int")
                builder.appendInt(prefix + "value", property.intValue);
            else if (property.type == "Float")
                builder.appendFloat(prefix + "value", property.floatValue);
            else
                builder.appendString(prefix + "value", property.stringValue);
        }
    }
    return builder.hash();
}

std::string unknownEngineSignature(const SceneNode& node)
{
    return SignatureBuilder().appendRaw("Unknown").appendString("type", node.type).hash();
}

std::string rootEngineSignature()
{
    return SignatureBuilder().appendRaw("root").hash();
}
}  // namespace

ScenePreviewRuntime::~ScenePreviewRuntime()
{
    reset();
}

void ScenePreviewRuntime::update(float deltaTime)
{
    if (m_root && m_root->isRunning())
        m_root->update(deltaTime);
}

void ScenePreviewRuntime::reset()
{
    if (m_renderTexture)
    {
        m_renderTexture->release();
        m_renderTexture = nullptr;
    }
    m_renderTextureRequestedWidth  = 0;
    m_renderTextureRequestedHeight = 0;
    m_renderTextureCapacityWidth   = 0;
    m_renderTextureCapacityHeight  = 0;

    if (m_root)
    {
        if (m_root->isRunning())
        {
            m_root->onExitTransitionDidStart();
            m_root->onExit();
        }
        m_root->removeAllChildrenWithCleanup(true);
        m_root->release();
    }

    m_root        = nullptr;
    m_contentRoot = nullptr;
    m_runtimeNodes.clear();
    m_documentKey.clear();
}

void ScenePreviewRuntime::sync(const SceneNode& rootNode,
                               const std::filesystem::path& documentKey,
                               const SceneCanvasFrame& frame,
                               const ImVec2& pan,
                               float zoom)
{
    ensureRoot();
    if (!m_root || !m_contentRoot)
        return;
    m_root->setContentSize(ax::Vec2(frame.canvasSize.x, frame.canvasSize.y));
    m_root->setAnchorPoint(ax::Vec2(0.0f, 0.0f));
    m_root->setScale(1.0f);
    m_root->setPosition(ax::Vec2::ZERO);

    m_contentRoot->setPosition(ax::Vec2(frame.canvasSize.x * 0.5f + pan.x, frame.canvasSize.y * 0.5f - pan.y));
    m_contentRoot->setScale(zoom);
    m_contentRoot->setVisible(rootNode.visible);

    if (m_documentKey != documentKey)
    {
        m_contentRoot->removeAllChildrenWithCleanup(true);
        m_runtimeNodes.clear();
        m_documentKey = documentKey;
    }

    syncTree(rootNode);
}

void ScenePreviewRuntime::renderToTexture(const SceneCanvasFrame& frame)
{
#if AXMOL_FIGHTER_EDITOR_LAYER_PREVIEW_RENDER_TEXTURE
    if (!m_root)
        return;

    m_renderTextureRequestedWidth  = std::max(1, static_cast<int>(std::ceil(frame.canvasSize.x)));
    m_renderTextureRequestedHeight = std::max(1, static_cast<int>(std::ceil(frame.canvasSize.y)));

    const bool needsRenderTexture = !m_renderTexture || m_renderTextureRequestedWidth > m_renderTextureCapacityWidth ||
                                    m_renderTextureRequestedHeight > m_renderTextureCapacityHeight;
    if (needsRenderTexture)
    {
        if (m_renderTexture)
            m_renderTexture->release();

        const int capacityWidth =
            expandedRenderTextureCapacity(m_renderTextureRequestedWidth, m_renderTextureCapacityWidth);
        const int capacityHeight =
            expandedRenderTextureCapacity(m_renderTextureRequestedHeight, m_renderTextureCapacityHeight);
        m_renderTexture =
            ax::RenderTexture::create(capacityWidth, capacityHeight, ax::backend::PixelFormat::RGBA8, false);
        if (m_renderTexture)
        {
            m_renderTexture->retain();
            m_renderTextureCapacityWidth  = capacityWidth;
            m_renderTextureCapacityHeight = capacityHeight;
        }
        else
        {
            m_renderTextureCapacityWidth  = 0;
            m_renderTextureCapacityHeight = 0;
        }
    }

    if (!m_renderTexture)
        return;

    m_renderTexture->beginWithClear(0.0f, 0.0f, 0.0f, 0.0f);
    m_root->visit();
    m_renderTexture->end();
#else
    (void)frame;
#endif
}

ax::Node* ScenePreviewRuntime::rootNode() const
{
    return m_root;
}

ax::RenderTexture* ScenePreviewRuntime::renderTexture() const
{
    return m_renderTexture;
}

int ScenePreviewRuntime::renderTextureRequestedWidth() const
{
    return m_renderTextureRequestedWidth;
}

int ScenePreviewRuntime::renderTextureRequestedHeight() const
{
    return m_renderTextureRequestedHeight;
}

int ScenePreviewRuntime::renderTextureCapacityWidth() const
{
    return m_renderTextureCapacityWidth;
}

int ScenePreviewRuntime::renderTextureCapacityHeight() const
{
    return m_renderTextureCapacityHeight;
}

void ScenePreviewRuntime::ensureRoot()
{
    if (m_root && m_contentRoot)
        return;

    reset();
    m_root = ax::Node::create();
    m_root->retain();
    m_root->setVisible(true);

    m_contentRoot = ax::Node::create();
    m_contentRoot->setName("ScenePreviewContentRoot");
    m_root->addChild(m_contentRoot);

    m_root->onEnter();
    m_root->onEnterTransitionDidFinish();
}

bool ScenePreviewRuntime::syncTree(const SceneNode& node)
{
    if (!m_contentRoot)
        return false;

    RuntimeNodeRecord& rootRecord = m_runtimeNodes[node.id];
    rootRecord.node               = m_contentRoot;
    rootRecord.parentId.clear();
    rootRecord.type            = node.type;
    rootRecord.engineSignature = rootEngineSignature();
    rootRecord.updateSignature = sceneNodeUpdateSignature(node);
    rootRecord.isError         = false;
    return syncRuntimeChildren(node, *m_contentRoot, 0);
}

bool ScenePreviewRuntime::syncRuntimeChildren(const SceneNode& node, ax::Node& runtimeNode, int depth)
{
    std::vector<std::string> desiredIds;
    desiredIds.reserve(node.children.size());
    for (const SceneNode& child : node.children)
        desiredIds.push_back(child.id);

    if (auto parentIt = m_runtimeNodes.find(node.id); parentIt != m_runtimeNodes.end())
    {
        const std::vector<std::string> previousIds = parentIt->second.childIds;
        for (const std::string& previousId : previousIds)
        {
            if (std::find(desiredIds.begin(), desiredIds.end(), previousId) == desiredIds.end())
                removeRuntimeSubtree(previousId);
        }
    }

    for (const SceneNode& child : node.children)
    {
        if (!syncRuntimeNode(child, node.id, runtimeNode, depth))
            return false;
    }

    if (!syncRuntimeChildOrder(node, runtimeNode))
        return false;

    auto parentIt = m_runtimeNodes.find(node.id);
    if (parentIt != m_runtimeNodes.end())
        parentIt->second.childIds = std::move(desiredIds);
    return true;
}

bool ScenePreviewRuntime::syncRuntimeNode(const SceneNode& node,
                                          const std::string& parentId,
                                          ax::Node& parentRuntime,
                                          int depth)
{
    const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(node.type);
    const SceneNodeRuntimeContext context{depth};
    const std::string engineSignature = editor ? editor->engineSignature(node, context) : unknownEngineSignature(node);
    const std::string updateSignature = sceneNodeUpdateSignature(node);

    auto runtimeIt = m_runtimeNodes.find(node.id);
    if (runtimeIt == m_runtimeNodes.end())
    {
        if (!replaceRuntimeNode(node, parentId, parentRuntime, depth))
            return false;
        runtimeIt = m_runtimeNodes.find(node.id);
    }
    else if (runtimeIt->second.parentId != parentId)
    {
        if (runtimeIt->second.node)
        {
            runtimeIt->second.node->retain();
            runtimeIt->second.node->removeFromParentAndCleanup(false);
            parentRuntime.addChild(runtimeIt->second.node);
            runtimeIt->second.node->release();
        }
        runtimeIt->second.parentId = parentId;
    }

    if (runtimeIt == m_runtimeNodes.end() || !runtimeIt->second.node)
        return false;

    if (runtimeIt->second.type != node.type || runtimeIt->second.engineSignature != engineSignature)
    {
        if (!replaceRuntimeNode(node, parentId, parentRuntime, depth))
            return false;
        runtimeIt = m_runtimeNodes.find(node.id);
    }

    if (runtimeIt == m_runtimeNodes.end() || !runtimeIt->second.node)
        return false;

    ax::Node* runtimeNode = runtimeIt->second.node;
    syncRuntimeTransform(*runtimeNode, node);

    if (runtimeIt->second.updateSignature != updateSignature)
    {
        bool updated = false;
        if (editor && !runtimeIt->second.isError)
            updated = editor->updateEngineNode(*runtimeNode, node, context);
        if (!updated)
        {
            if (!replaceRuntimeNode(node, parentId, parentRuntime, depth))
                return false;
            runtimeIt = m_runtimeNodes.find(node.id);
            if (runtimeIt == m_runtimeNodes.end() || !runtimeIt->second.node)
                return false;
            runtimeNode = runtimeIt->second.node;
            syncRuntimeTransform(*runtimeNode, node);
        }
        else
        {
            runtimeIt->second.updateSignature = updateSignature;
        }
    }

    return syncRuntimeChildren(node, *runtimeNode, depth + 1);
}

bool ScenePreviewRuntime::syncRuntimeChildOrder(const SceneNode& node, ax::Node& runtimeNode)
{
    std::vector<ax::Node*> orderedChildren;
    orderedChildren.reserve(node.children.size());
    for (const SceneNode& child : node.children)
    {
        auto childIt = m_runtimeNodes.find(child.id);
        if (childIt == m_runtimeNodes.end() || childIt->second.parentId != node.id || !childIt->second.node)
            return false;
        orderedChildren.push_back(childIt->second.node);
    }

    for (ax::Node* childNode : orderedChildren)
    {
        childNode->retain();
        childNode->removeFromParentAndCleanup(false);
    }

    for (ax::Node* childNode : orderedChildren)
    {
        runtimeNode.addChild(childNode);
        childNode->release();
    }
    return true;
}

void ScenePreviewRuntime::removeRuntimeSubtree(const std::string& nodeId)
{
    auto it = m_runtimeNodes.find(nodeId);
    if (it == m_runtimeNodes.end())
        return;

    const std::vector<std::string> childIds = it->second.childIds;
    for (const std::string& childId : childIds)
        removeRuntimeSubtree(childId);

    if (it->second.node && it->second.node != m_contentRoot)
        it->second.node->removeFromParentAndCleanup(true);
    m_runtimeNodes.erase(it);
}

bool ScenePreviewRuntime::replaceRuntimeNode(const SceneNode& node,
                                             const std::string& parentId,
                                             ax::Node& parentRuntime,
                                             int depth)
{
    const SceneNodeRuntimeContext context{depth};
    const SceneNodeEditor* editor = SceneNodeEditorRegistry::instance().find(node.type);
    std::string errorMessage;
    ax::Node* nextNode = nullptr;
    bool isError       = false;
    if (editor)
        std::tie(errorMessage, nextNode) = editor->createEngineNode(node, context);
    else
        errorMessage = "Unknown node type: " + node.type;

    if (!nextNode)
    {
        nextNode = createRuntimeErrorNode(node, errorMessage.empty() ? "Failed to create " + node.type : errorMessage);
        isError  = true;
    }
    if (!nextNode)
        return false;

    std::vector<std::string> childIds;
    std::vector<ax::Node*> childNodes;
    auto existingIt = m_runtimeNodes.find(node.id);
    if (existingIt != m_runtimeNodes.end())
    {
        childIds = existingIt->second.childIds;
        childNodes.reserve(childIds.size());
        for (const std::string& childId : childIds)
        {
            auto childIt = m_runtimeNodes.find(childId);
            if (childIt != m_runtimeNodes.end() && childIt->second.node)
            {
                childIt->second.node->retain();
                childIt->second.node->removeFromParentAndCleanup(false);
                childNodes.push_back(childIt->second.node);
            }
        }

        if (existingIt->second.node && existingIt->second.node != m_contentRoot)
            existingIt->second.node->removeFromParentAndCleanup(true);
    }

    parentRuntime.addChild(nextNode);
    for (ax::Node* childNode : childNodes)
    {
        nextNode->addChild(childNode);
        childNode->release();
    }

    syncRuntimeTransform(*nextNode, node);
    RuntimeNodeRecord& record = m_runtimeNodes[node.id];
    record.node               = nextNode;
    record.parentId           = parentId;
    record.type               = node.type;
    record.engineSignature    = editor ? editor->engineSignature(node, context) : unknownEngineSignature(node);
    record.updateSignature    = sceneNodeUpdateSignature(node);
    record.isError            = isError;
    record.childIds           = std::move(childIds);
    return true;
}

ax::Node* ScenePreviewRuntime::createRuntimeErrorNode(const SceneNode& node, const std::string& message)
{
    ax::Node* container = ax::Node::create();
    if (!container)
        return nullptr;

    const float width  = std::max(1.0f, node.size.width);
    const float height = std::max(1.0f, node.size.height);

    if (auto* background = ax::DrawNode::create())
    {
        background->drawSolidRect(ax::Vec2(0.0f, 0.0f), ax::Vec2(width, height),
                                  ax::Color4F(0.86f, 0.12f, 0.10f, 0.82f));
        container->addChild(background);
    }

    if (auto* label = ax::Label::createWithSystemFont(message, "Arial", 14.0f))
    {
        label->setTextColor(ax::Color4B::WHITE);
        label->setDimensions(std::max(1.0f, width - 8.0f), std::max(1.0f, height - 8.0f));
        label->setAlignment(ax::TextHAlignment::CENTER, ax::TextVAlignment::CENTER);
        label->setAnchorPoint(ax::Vec2(0.5f, 0.5f));
        label->setPosition(ax::Vec2(width * 0.5f, height * 0.5f));
        container->addChild(label);
    }

    return container;
}

void ScenePreviewRuntime::syncRuntimeTransform(ax::Node& runtimeNode, const SceneNode& node)
{
    const auto colorByte = [](float value) {
        return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    };
    runtimeNode.setName(node.name);
    runtimeNode.setVisible(node.visible);
    runtimeNode.setPosition(ax::Vec2(node.position.x, node.position.y));
    runtimeNode.setPositionZ(node.positionZ);
    const bool spriteUsesPolygonSize = node.type == "Sprite" && node.sprite.renderType == "Tiled";
    if (dynamic_cast<ax::Label*>(&runtimeNode) == nullptr && !spriteUsesPolygonSize)
        runtimeNode.setContentSize(ax::Vec2(std::max(1.0f, node.size.width), std::max(1.0f, node.size.height)));
    runtimeNode.setAnchorPoint(ax::Vec2(node.anchor.x, node.anchor.y));
    runtimeNode.setScale(node.scale.x, node.scale.y);
    runtimeNode.setRotation(node.rotation);
    runtimeNode.setSkewX(node.skew.x);
    runtimeNode.setSkewY(node.skew.y);
    runtimeNode.setColor(ax::Color3B(colorByte(node.color.r), colorByte(node.color.g), colorByte(node.color.b)));
    runtimeNode.setOpacity(static_cast<uint8_t>(std::clamp(node.opacity, 0, 255)));
}
}  // namespace editor
