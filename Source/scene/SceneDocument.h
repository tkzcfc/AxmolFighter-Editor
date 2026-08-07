#pragma once

#include "scene/SceneTypes.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <optional>
#include <string>
#include <vector>

namespace editor
{
enum class SceneNodeSiblingMove
{
    Up,
    Down,
    Top,
    Bottom,
};

enum class SceneNodeParentAlignment
{
    Left,
    HorizontalCenter,
    Right,
    Top,
    VerticalCenter,
    Bottom,
};

struct SceneNodeCopyData
{
    SceneNode node;
    std::string parentNodeId;
};

class SceneDocument
{
public:
    virtual ~SceneDocument() = default;

    SceneNode& root();
    const SceneNode& root() const;
    SceneNode* selectedNode();
    const SceneNode* selectedNode() const;
    const std::string& selectedNodeId() const;
    bool selectNode(const std::string& id);
    void clearNodeSelection();

    SceneNode* addChildOfType(const std::string& parentId, const std::string& typeId);
    SceneNode* addChildNode(const std::string& parentId);
    SceneNode* addChildSprite(const std::string& parentId);
    bool canAddChild(const std::string& parentId) const;
    bool canReparent(const std::string& nodeId, const std::string& newParentId) const;
    bool reparentNode(const std::string& nodeId, const std::string& newParentId);
    bool canMoveNodeSiblingOrder(const std::string& nodeId, SceneNodeSiblingMove move) const;
    bool moveNodeSiblingOrder(const std::string& nodeId, SceneNodeSiblingMove move);
    bool canAlignSelectedNodeToParent() const;
    bool alignSelectedNodeToParent(SceneNodeParentAlignment alignment);
    bool canDeleteNode(const std::string& nodeId) const;
    bool deleteNode(const std::string& nodeId);
    bool copyNode(const std::string& nodeId, SceneNodeCopyData& outCopy) const;
    bool copySelectedNode(SceneNodeCopyData& outCopy) const;
    SceneNode* pasteNodeCopy(const SceneNodeCopyData& copy,
                             const std::string& preferredParentId,
                             const SceneVec2& rootPositionOffset);
    int nodeCount() const;

    bool canUndo() const;
    bool undo();
    bool beginUndoTransaction();
    void commitUndoTransaction();
    void cancelUndoTransaction();
    bool hasActiveUndoTransaction() const;
    void markDirty();

protected:
    struct SceneSnapshot
    {
        SceneNode root;
        std::string selectedNodeId = "root";
        int nextNodeSerial = 1;
        std::string extensionJson;
    };

    void resetSceneToDefault();
    void resetSceneUndoHistory();
    void markSceneSaved();
    bool isSceneDirty() const;
    void setSceneDirty(bool dirty);

    bool loadSceneRootFromJson(const rapidjson::Value& value);
    void writeSceneRootJson(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const;
    void scanSceneNodeIds();

    SceneNode* findNode(const std::string& id);
    const SceneNode* findNode(const std::string& id) const;
    SceneNode* findParent(const std::string& id);
    const SceneNode* findParent(const std::string& id) const;

    virtual void onSceneNodeSelected();
    virtual void onSceneNodeSelectionCleared();
    virtual std::string captureSceneExtensionJson() const;
    virtual void restoreSceneExtensionJson(const std::string& json);
    virtual void writeSceneExtensionSnapshot(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
                                             const std::string& extensionJson) const;
    virtual std::string sceneDirtyExtensionJson(const std::string& extensionJson) const;

private:
    SceneSnapshot captureSceneSnapshot() const;
    void restoreSceneSnapshot(const SceneSnapshot& snapshot);
    std::string sceneSnapshotKey(const SceneSnapshot& snapshot) const;
    std::string sceneDirtyKey(const SceneSnapshot& snapshot) const;
    bool containsNode(const SceneNode& node, const std::string& id) const;
    std::optional<SceneNode> detachNode(const std::string& id);
    void assignFreshNodeIds(SceneNode& node);
    std::string nextNodeId();
    void scanNodeIds(const SceneNode& node);
    std::string nextUniqueNodeName(const std::string& baseName) const;

    SceneNode m_root;
    std::string m_selectedNodeId = "root";
    int m_nextNodeSerial = 1;
    SceneSnapshot m_undoCheckpoint;
    std::string m_undoCheckpointKey;
    std::string m_savedStateKey;
    std::vector<SceneSnapshot> m_undoStack;
    SceneSnapshot m_undoTransactionSnapshot;
    std::string m_undoTransactionKey;
    bool m_undoTransactionActive = false;
    bool m_dirty = false;
};
}  // namespace editor
