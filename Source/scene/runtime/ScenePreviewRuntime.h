#pragma once

#include "scene/SceneTypes.h"
#include "scene/canvas/SceneCanvas.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ax
{
class Node;
class RenderTexture;
}

namespace editor
{
class ScenePreviewRuntime final
{
public:
    ~ScenePreviewRuntime();

    void update(float deltaTime);
    void reset();
    void sync(const SceneNode& rootNode,
              const std::filesystem::path& documentKey,
              const SceneCanvasFrame& frame,
              const ImVec2& pan,
              float zoom);
    void renderToTexture(const SceneCanvasFrame& frame);

    ax::Node* rootNode() const;
    ax::RenderTexture* renderTexture() const;
    int renderTextureRequestedWidth() const;
    int renderTextureRequestedHeight() const;
    int renderTextureCapacityWidth() const;
    int renderTextureCapacityHeight() const;

private:
    struct RuntimeNodeRecord
    {
        ax::Node* node = nullptr;
        std::string parentId;
        std::string type;
        std::string engineSignature;
        std::string updateSignature;
        bool isError = false;
        std::vector<std::string> childIds;
    };

    void ensureRoot();
    bool syncTree(const SceneNode& node);
    bool syncRuntimeNode(const SceneNode& node, const std::string& parentId, ax::Node& parentRuntime, int depth);
    bool syncRuntimeChildren(const SceneNode& node, ax::Node& runtimeNode, int depth);
    bool syncRuntimeChildOrder(const SceneNode& node, ax::Node& runtimeNode);
    void removeRuntimeSubtree(const std::string& nodeId);
    bool replaceRuntimeNode(const SceneNode& node, const std::string& parentId, ax::Node& parentRuntime, int depth);
    ax::Node* createRuntimeErrorNode(const SceneNode& node, const std::string& message);
    void syncRuntimeTransform(ax::Node& runtimeNode, const SceneNode& node);

    ax::Node* m_root = nullptr;
    ax::Node* m_contentRoot = nullptr;
    ax::RenderTexture* m_renderTexture = nullptr;
    int m_renderTextureRequestedWidth = 0;
    int m_renderTextureRequestedHeight = 0;
    int m_renderTextureCapacityWidth = 0;
    int m_renderTextureCapacityHeight = 0;
    std::unordered_map<std::string, RuntimeNodeRecord> m_runtimeNodes;
    std::filesystem::path m_documentKey;
};
}  // namespace editor
